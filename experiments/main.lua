-- I wish we could just say `lua blah.so` to run the C code in our experiments.
-- But since we can't do that then we need to use a "main.lua" entrypoint.
local modname = assert(arg[1])
local N = arg[2] and tonumber(arg[2])

-- As a convenience, allow either a module name or a file name
-- This plays nice with the shell tab-completion.
local prefix, _ = string.match(modname, "(.*)%.(.*)")
if prefix then
    modname = prefix
end

local main = require(modname)
main(N)
