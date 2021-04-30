#!/usr/bin/lua

print("Benchmark","Implementation","N","Time")
while true do
    local line1 = io.read("*l")
    if not line1 then break end
    local line2 = assert(io.read("*l"))
    local _ = assert(io.read("*l"))
    local _ = assert(io.read("*l"))
    local _ = assert(io.read("*l"))

    local name, impl, i = string.match(line1, "RUN (%S+) (%S+) (%S+)")
    local time          = string.match(line2, "real (%S+)")

    print(table.concat({name,impl,i,time}, ","))
end

