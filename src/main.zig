const std = @import("std");

const default_host = "0.0.0.0";
const default_port: u16 = 8000;
const server_name = "3s";
const read_buffer_size = 4096;
const file_buffer_size = 8192;

const Config = struct {
    host: []const u8 = default_host,
    port: u16 = default_port,
    directory: []const u8 = ".",
    user: ?[]const u8 = null,
    password: ?[]const u8 = null,

    fn deinit(self: Config, allocator: std.mem.Allocator) void {
        if (self.host.ptr != default_host.ptr) allocator.free(self.host);
        if (self.directory.ptr != ".".ptr) allocator.free(self.directory);
        if (self.user) |user| allocator.free(user);
        if (self.password) |password| allocator.free(password);
    }
};

const Status = struct {
    code: u16,
    text: []const u8,
};

const Request = struct {
    method: []const u8,
    raw_target: []const u8,
    path: []const u8,
    authorization: ?[]const u8,
};

const RequestContext = struct {
    connection: std.net.Server.Connection,
    root_path: []const u8,
    user: ?[]const u8,
    password: ?[]const u8,
    client_addr: [128]u8,
    client_addr_len: usize,
};

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer if (gpa.deinit() == .leak) std.debug.print("memory leak detected\n", .{});
    const allocator = gpa.allocator();

    const config = try parseArgs(allocator);
    defer config.deinit(allocator);
    const root_path = try std.fs.realpathAlloc(allocator, config.directory);
    defer allocator.free(root_path);

    const address = try std.net.Address.parseIp(config.host, config.port);
    var server = try address.listen(.{
        .reuse_address = true,
        .force_nonblocking = false,
    });
    defer server.deinit();

    std.debug.print("{s: >35} started on {s}:{d}, with socket {d}\n", .{
        "",
        config.host,
        config.port,
        server.stream.handle,
    });

    while (true) {
        const conn = server.accept() catch |err| {
            std.debug.print("accept failed: {s}\n", .{@errorName(err)});
            continue;
        };

        var ctx = try allocator.create(RequestContext);
        errdefer allocator.destroy(ctx);

        ctx.* = .{
            .connection = conn,
            .root_path = root_path,
            .user = config.user,
            .password = config.password,
            .client_addr = undefined,
            .client_addr_len = 0,
        };
        const addr = std.fmt.bufPrint(&ctx.client_addr, "{f}", .{conn.address}) catch "unknown";
        ctx.client_addr_len = addr.len;

        const thread = std.Thread.spawn(.{}, handleConnection, .{ allocator, ctx }) catch |err| {
            std.debug.print("failed creating thread: {s}\n", .{@errorName(err)});
            conn.stream.close();
            allocator.destroy(ctx);
            continue;
        };
        thread.detach();
    }
}

fn parseArgs(allocator: std.mem.Allocator) !Config {
    var config = Config{};
    errdefer config.deinit(allocator);
    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    var i: usize = 1;
    while (i < args.len) {
        const arg = args[i];
        if (std.mem.eql(u8, arg, "--help") or std.mem.eql(u8, arg, "-h")) {
            printUsage();
            std.process.exit(0);
        } else if (std.mem.eql(u8, arg, "--user") or std.mem.eql(u8, arg, "-u")) {
            i += 1;
            if (i >= args.len) return error.MissingUserArgument;
            config.user = try allocator.dupe(u8, args[i]);
        } else if (std.mem.eql(u8, arg, "--password") or std.mem.eql(u8, arg, "-P")) {
            i += 1;
            if (i >= args.len) return error.MissingPasswordArgument;
            config.password = try allocator.dupe(u8, args[i]);
        } else if (std.mem.eql(u8, arg, "--host") or std.mem.eql(u8, arg, "-H")) {
            i += 1;
            if (i >= args.len) return error.MissingHostArgument;
            config.host = try allocator.dupe(u8, args[i]);
        } else if (std.mem.eql(u8, arg, "--port") or std.mem.eql(u8, arg, "-p")) {
            i += 1;
            if (i >= args.len) return error.MissingPortArgument;
            const port_str = args[i];
            config.port = try std.fmt.parseUnsigned(u16, port_str, 10);
            if (config.port == 0) return error.InvalidPort;
        } else if (std.mem.startsWith(u8, arg, "-")) {
            printUsage();
            return error.InvalidArgument;
        } else {
            config.directory = try allocator.dupe(u8, arg);
            if (i + 1 != args.len) return error.TooManyArguments;
        }
        i += 1;
    }

    if ((config.user == null) != (config.password == null)) {
        return error.IncompleteCredentials;
    }

    return config;
}

fn printUsage() void {
    std.debug.print(
        \\Usage: {s} [options] [directory]
        \\Options:
        \\  --user <name>       Username
        \\  --password <pass>   Password
        \\  --host <host>       Host (default: 0.0.0.0)
        \\  --port <port>       Port (default: 8000, must be 1-65535)
        \\  [directory]         Target directory (default: .)
        \\
    , .{server_name});
}

fn handleConnection(allocator: std.mem.Allocator, ctx: *RequestContext) void {
    defer allocator.destroy(ctx);
    defer ctx.connection.stream.close();

    var arena = std.heap.ArenaAllocator.init(allocator);
    defer arena.deinit();
    const arena_alloc = arena.allocator();

    _ = handleConnectionInner(arena_alloc, ctx) catch |err| {
        std.debug.print("request handling failed: {s}\n", .{@errorName(err)});
    };
}

fn handleConnectionInner(allocator: std.mem.Allocator, ctx: *RequestContext) !Status {
    var reader = SocketReader.init(ctx.connection.stream);
    const request = try parseRequest(allocator, &reader);

    if (ctx.user != null and ctx.password != null) {
        if (!isAuthorized(allocator, request.authorization, ctx.user.?, ctx.password.?)) {
            const status = Status{ .code = 401, .text = "Unauthorized" };
            try writeStatus(ctx.connection.stream, status);
            try writeHeader(ctx.connection.stream, "WWW-Authenticate", "Basic realm=\"3s\"");
            try writeHeader(ctx.connection.stream, "Content-Length", "0");
            try endHeaders(ctx.connection.stream);
            logRequest(ctx.client_addr[0..ctx.client_addr_len], status, request.path);
            return status;
        }
    }

    if (!std.ascii.eqlIgnoreCase(request.method, "GET") and !std.ascii.eqlIgnoreCase(request.method, "HEAD")) {
        const status = Status{ .code = 405, .text = "Method Not Allowed" };
        try writeStatus(ctx.connection.stream, status);
        try writeHeader(ctx.connection.stream, "Allow", "GET, HEAD");
        try writeHeader(ctx.connection.stream, "Content-Length", "0");
        try endHeaders(ctx.connection.stream);
        logRequest(ctx.client_addr[0..ctx.client_addr_len], status, request.path);
        return status;
    }

    const path_info = buildFilesystemPath(allocator, ctx.root_path, request.path) catch {
        const status = Status{ .code = 403, .text = "Forbidden" };
        try writeStatus(ctx.connection.stream, status);
        try writeHeader(ctx.connection.stream, "Content-Length", "0");
        try endHeaders(ctx.connection.stream);
        logRequest(ctx.client_addr[0..ctx.client_addr_len], status, request.path);
        return status;
    };

    const status = try servePath(allocator, ctx.connection.stream, path_info, request.path, std.ascii.eqlIgnoreCase(request.method, "HEAD"));
    logRequest(ctx.client_addr[0..ctx.client_addr_len], status, request.path);
    return status;
}

fn parseRequest(allocator: std.mem.Allocator, reader: *SocketReader) !Request {
    const request_line = try reader.readLineAlloc(allocator, read_buffer_size);
    if (request_line.len == 0) return error.EmptyRequest;

    var parts = std.mem.tokenizeScalar(u8, request_line, ' ');
    const method = parts.next() orelse return error.InvalidRequestLine;
    const raw_target = parts.next() orelse return error.InvalidRequestLine;
    _ = parts.next() orelse return error.InvalidRequestLine;

    var authorization: ?[]const u8 = null;
    while (true) {
        const line = try reader.readLineAlloc(allocator, read_buffer_size);
        if (line.len == 0) break;

        if (std.mem.indexOfScalar(u8, line, ':')) |idx| {
            const name = std.mem.trim(u8, line[0..idx], " \t");
            const value = std.mem.trim(u8, line[idx + 1 ..], " \t");
            if (std.ascii.eqlIgnoreCase(name, "Authorization")) {
                authorization = value;
            }
        }
    }

    const clean_target = raw_target[0 .. std.mem.indexOfScalar(u8, raw_target, '?') orelse raw_target.len];
    const decoded = try percentDecodeAlloc(allocator, clean_target);

    return .{
        .method = method,
        .raw_target = raw_target,
        .path = decoded,
        .authorization = authorization,
    };
}

fn servePath(
    allocator: std.mem.Allocator,
    stream: std.net.Stream,
    absolute_path: []const u8,
    request_path: []const u8,
    head_only: bool,
) !Status {
    const metadata = std.fs.cwd().statFile(absolute_path) catch |err| switch (err) {
        error.FileNotFound => {
            const status = Status{ .code = 404, .text = "Not Found" };
            try writeStatus(stream, status);
            try writeHeader(stream, "Content-Length", "0");
            try endHeaders(stream);
            return status;
        },
        else => return err,
    };

    switch (metadata.kind) {
        .directory => {
            const status = Status{ .code = 200, .text = "OK" };
            try writeStatus(stream, status);
            try sendDirectory(allocator, stream, absolute_path, request_path, head_only);
            return status;
        },
        .file => {
            const status = Status{ .code = 200, .text = "OK" };
            try writeStatus(stream, status);
            try sendFile(stream, absolute_path, metadata.size, head_only);
            return status;
        },
        else => {
            const unsupported = Status{ .code = 403, .text = "Forbidden" };
            try writeStatus(stream, unsupported);
            try writeHeader(stream, "Content-Length", "0");
            try endHeaders(stream);
            return unsupported;
        },
    }
}

fn sendFile(stream: std.net.Stream, absolute_path: []const u8, file_size: u64, head_only: bool) !void {
    var file = try std.fs.openFileAbsolute(absolute_path, .{ .mode = .read_only });
    defer file.close();

    var size_buf: [32]u8 = undefined;
    const content_length = try std.fmt.bufPrint(&size_buf, "{d}", .{file_size});

    try writeHeader(stream, "Content-Type", getMimeType(absolute_path));
    try writeHeader(stream, "Content-Length", content_length);
    try endHeaders(stream);

    if (head_only) return;

    var buf: [file_buffer_size]u8 = undefined;
    while (true) {
        const bytes_read = try file.read(&buf);
        if (bytes_read == 0) break;
        try stream.writeAll(buf[0..bytes_read]);
    }
}

fn sendDirectory(
    allocator: std.mem.Allocator,
    stream: std.net.Stream,
    absolute_path: []const u8,
    request_path: []const u8,
    head_only: bool,
) !void {
    var dir = try std.fs.openDirAbsolute(absolute_path, .{ .iterate = true });
    defer dir.close();

    var body: std.ArrayList(u8) = .empty;
    defer body.deinit(allocator);
    const writer = body.writer(allocator);

    try writer.writeAll(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>a{text-decoration:none;}body{font-family:monospace;max-width:960px;margin:2rem auto;padding:0 1rem;}table{width:100%;border-collapse:collapse;}th,td{text-align:left;padding:0.25rem 0.5rem;}thead tr{border-bottom:1px solid #bbb;}tbody tr:nth-child(odd){background:#fafafa;}</style></head><body><div><a href=\"/\"><strong>[Root]</strong></a></div><hr /><table><thead><tr><th>Name</th><th>Last modified (UTC)</th><th>Size</th></tr></thead><tbody>"
    );

    const normalized_url = try normalizeDirectoryUrl(allocator, request_path);
    var iterator = dir.iterate();
    while (try iterator.next()) |entry| {
        const child_stat = dir.statFile(entry.name) catch continue;
        const display_name = if (entry.kind == .directory)
            try std.fmt.allocPrint(allocator, "{s}/", .{entry.name})
        else
            try allocator.dupe(u8, entry.name);
        defer allocator.free(display_name);

        const escaped_name = try htmlEscapeAlloc(allocator, display_name);
        defer allocator.free(escaped_name);

        const encoded_name = try percentEncodeAlloc(allocator, display_name);
        defer allocator.free(encoded_name);

        const timestamp = try formatTimestampUtc(allocator, child_stat.mtime);
        defer allocator.free(timestamp);

        const size_text = if (entry.kind == .directory)
            try allocator.dupe(u8, "-")
        else
            try formatSizeAlloc(allocator, child_stat.size);
        defer allocator.free(size_text);

        try writer.print(
            "<tr><td><a href=\"{s}{s}\">{s}</a></td><td>{s}</td><td>{s}</td></tr>",
            .{ normalized_url, encoded_name, escaped_name, timestamp, size_text },
        );
    }

    try writer.writeAll("</tbody></table></body></html>");

    var size_buf: [32]u8 = undefined;
    const content_length = try std.fmt.bufPrint(&size_buf, "{d}", .{body.items.len});

    try writeHeader(stream, "Content-Type", "text/html; charset=utf-8");
    try writeHeader(stream, "Content-Length", content_length);
    try endHeaders(stream);
    if (!head_only) {
        try stream.writeAll(body.items);
    }
}

fn normalizeDirectoryUrl(allocator: std.mem.Allocator, request_path: []const u8) ![]const u8 {
    if (request_path.len == 0 or std.mem.eql(u8, request_path, "/")) {
        return allocator.dupe(u8, "/");
    }
    if (request_path[request_path.len - 1] == '/') {
        return allocator.dupe(u8, request_path);
    }
    return std.fmt.allocPrint(allocator, "{s}/", .{request_path});
}

fn buildFilesystemPath(allocator: std.mem.Allocator, root_path: []const u8, request_path: []const u8) ![]const u8 {
    if (request_path.len == 0 or request_path[0] != '/') return error.InvalidPath;

    var out: std.ArrayList(u8) = .empty;
    errdefer out.deinit(allocator);
    try out.appendSlice(allocator, root_path);

    var it = std.mem.splitScalar(u8, request_path[1..], '/');
    while (it.next()) |segment| {
        if (segment.len == 0 or std.mem.eql(u8, segment, ".")) continue;
        if (std.mem.eql(u8, segment, "..")) return error.DirectoryTraversal;
        if (out.items.len == 0 or out.items[out.items.len - 1] != std.fs.path.sep) {
            try out.append(allocator, std.fs.path.sep);
        }
        try out.appendSlice(allocator, segment);
    }

    return out.toOwnedSlice(allocator);
}

fn isAuthorized(
    allocator: std.mem.Allocator,
    header_value: ?[]const u8,
    user: []const u8,
    password: []const u8,
) bool {
    const header = header_value orelse return false;
    const prefix = "Basic ";
    if (!std.mem.startsWith(u8, header, prefix)) return false;

    const combined = std.fmt.allocPrint(allocator, "{s}:{s}", .{ user, password }) catch return false;
    defer allocator.free(combined);

    const encoded_len = std.base64.standard.Encoder.calcSize(combined.len);
    const encoded = allocator.alloc(u8, encoded_len) catch return false;
    defer allocator.free(encoded);
    _ = std.base64.standard.Encoder.encode(encoded, combined);

    return std.mem.eql(u8, header[prefix.len..], encoded);
}

fn percentDecodeAlloc(allocator: std.mem.Allocator, input: []const u8) ![]const u8 {
    var out: std.ArrayList(u8) = .empty;
    defer out.deinit(allocator);

    var i: usize = 0;
    while (i < input.len) : (i += 1) {
        const c = input[i];
        if (c == '%' and i + 2 < input.len) {
            const hi = std.fmt.charToDigit(input[i + 1], 16) catch return error.InvalidPercentEncoding;
            const lo = std.fmt.charToDigit(input[i + 2], 16) catch return error.InvalidPercentEncoding;
            try out.append(allocator, @as(u8, @intCast(hi * 16 + lo)));
            i += 2;
        } else if (c == '+') {
            try out.append(allocator, ' ');
        } else {
            try out.append(allocator, c);
        }
    }

    return out.toOwnedSlice(allocator);
}

fn percentEncodeAlloc(allocator: std.mem.Allocator, input: []const u8) ![]const u8 {
    var out: std.ArrayList(u8) = .empty;
    defer out.deinit(allocator);
    const writer = out.writer(allocator);

    for (input) |c| {
        if (std.ascii.isAlphanumeric(c) or c == '-' or c == '_' or c == '.' or c == '~' or c == '/') {
            try out.append(allocator, c);
        } else {
            try writer.print("%{X:0>2}", .{c});
        }
    }

    return out.toOwnedSlice(allocator);
}

fn htmlEscapeAlloc(allocator: std.mem.Allocator, input: []const u8) ![]const u8 {
    var out: std.ArrayList(u8) = .empty;
    defer out.deinit(allocator);

    for (input) |c| {
        switch (c) {
            '&' => try out.appendSlice(allocator, "&amp;"),
            '<' => try out.appendSlice(allocator, "&lt;"),
            '>' => try out.appendSlice(allocator, "&gt;"),
            '"' => try out.appendSlice(allocator, "&quot;"),
            '\'' => try out.appendSlice(allocator, "&#39;"),
            else => try out.append(allocator, c),
        }
    }

    return out.toOwnedSlice(allocator);
}

fn formatSizeAlloc(allocator: std.mem.Allocator, size: u64) ![]const u8 {
    if (size >= 1024 * 1024 * 1024) {
        return std.fmt.allocPrint(allocator, "{d:.2} GB", .{@as(f64, @floatFromInt(size)) / (1024.0 * 1024.0 * 1024.0)});
    }
    if (size >= 1024 * 1024) {
        return std.fmt.allocPrint(allocator, "{d:.2} MB", .{@as(f64, @floatFromInt(size)) / (1024.0 * 1024.0)});
    }
    if (size >= 1024) {
        return std.fmt.allocPrint(allocator, "{d:.2} KB", .{@as(f64, @floatFromInt(size)) / 1024.0});
    }
    return std.fmt.allocPrint(allocator, "{d} B", .{size});
}

fn formatTimestampUtc(allocator: std.mem.Allocator, mtime_ns: i128) ![]const u8 {
    if (mtime_ns < 0) return allocator.dupe(u8, "before-epoch");

    const seconds: u64 = @intCast(@divFloor(mtime_ns, std.time.ns_per_s));
    const epoch_seconds = std.time.epoch.EpochSeconds{ .secs = seconds };
    const epoch_day = epoch_seconds.getEpochDay();
    const day_seconds = epoch_seconds.getDaySeconds();
    const year_day = epoch_day.calculateYearDay();
    const month_day = year_day.calculateMonthDay();

    return std.fmt.allocPrint(
        allocator,
        "{d:0>4}-{d:0>2}-{d:0>2} {d:0>2}:{d:0>2}:{d:0>2}",
        .{
            year_day.year,
            @intFromEnum(month_day.month),
            month_day.day_index + 1,
            day_seconds.getHoursIntoDay(),
            day_seconds.getMinutesIntoHour(),
            day_seconds.getSecondsIntoMinute(),
        },
    );
}

fn writeStatus(stream: std.net.Stream, status: Status) !void {
    var buf: [64]u8 = undefined;
    const line = try std.fmt.bufPrint(&buf, "HTTP/1.1 {d} {s}\r\n", .{ status.code, status.text });
    try stream.writeAll(line);
}

fn writeHeader(stream: std.net.Stream, key: []const u8, value: []const u8) !void {
    try stream.writeAll(key);
    try stream.writeAll(": ");
    try stream.writeAll(value);
    try stream.writeAll("\r\n");
}

fn endHeaders(stream: std.net.Stream) !void {
    try stream.writeAll("\r\n");
}

fn getMimeType(path: []const u8) []const u8 {
    const ext = std.fs.path.extension(path);
    if (ext.len <= 1) return "text/plain";

    const clean = ext[1..];
    if (std.ascii.eqlIgnoreCase(clean, "html") or std.ascii.eqlIgnoreCase(clean, "htm")) return "text/html";
    if (std.ascii.eqlIgnoreCase(clean, "jpeg") or std.ascii.eqlIgnoreCase(clean, "jpg")) return "image/jpeg";
    if (std.ascii.eqlIgnoreCase(clean, "png")) return "image/png";
    if (std.ascii.eqlIgnoreCase(clean, "gif")) return "image/gif";
    if (std.ascii.eqlIgnoreCase(clean, "css")) return "text/css";
    if (std.ascii.eqlIgnoreCase(clean, "js")) return "application/javascript";
    if (std.ascii.eqlIgnoreCase(clean, "json")) return "application/json";
    if (std.ascii.eqlIgnoreCase(clean, "txt")) return "text/plain";
    if (std.ascii.eqlIgnoreCase(clean, "mp3")) return "audio/mpeg";
    if (std.ascii.eqlIgnoreCase(clean, "mp4")) return "video/mp4";
    if (std.ascii.eqlIgnoreCase(clean, "wav")) return "audio/wav";
    if (std.ascii.eqlIgnoreCase(clean, "ogg")) return "audio/ogg";
    if (std.ascii.eqlIgnoreCase(clean, "pdf")) return "application/pdf";

    return "text/plain";
}

fn logRequest(client: []const u8, status: Status, path: []const u8) void {
    const now_ms = std.time.milliTimestamp();
    const seconds: u64 = @intCast(@divFloor(now_ms, std.time.ms_per_s));
    const millis: u64 = @intCast(@mod(now_ms, std.time.ms_per_s));

    const epoch_seconds = std.time.epoch.EpochSeconds{ .secs = seconds };
    const epoch_day = epoch_seconds.getEpochDay();
    const day_seconds = epoch_seconds.getDaySeconds();
    const year_day = epoch_day.calculateYearDay();
    const month_day = year_day.calculateMonthDay();

    std.debug.print(
        "{d:0>4}-{d:0>2}-{d:0>2} {d:0>2}:{d:0>2}:{d:0>2}.{d:0>3}\t{s: >22} {d: >3} {s}\n",
        .{
            year_day.year,
            @intFromEnum(month_day.month),
            month_day.day_index + 1,
            day_seconds.getHoursIntoDay(),
            day_seconds.getMinutesIntoHour(),
            day_seconds.getSecondsIntoMinute(),
            millis,
            client,
            status.code,
            path,
        },
    );
}

const SocketReader = struct {
    stream: std.net.Stream,

    fn init(stream: std.net.Stream) SocketReader {
        return .{ .stream = stream };
    }

    fn readLineAlloc(self: *SocketReader, allocator: std.mem.Allocator, limit: usize) ![]const u8 {
        var out: std.ArrayList(u8) = .empty;
        defer out.deinit(allocator);

        while (out.items.len < limit) {
            var buf: [1]u8 = undefined;
            const n = try self.stream.read(&buf);
            if (n == 0) break;

            const c = buf[0];
            if (c == '\n') break;
            if (c == '\r') {
                var next_buf: [1]u8 = undefined;
                const maybe_next = self.stream.read(&next_buf) catch 0;
                if (maybe_next == 1 and next_buf[0] != '\n') {
                    try out.append(allocator, next_buf[0]);
                }
                break;
            }

            try out.append(allocator, c);
        }

        return out.toOwnedSlice(allocator);
    }
};
