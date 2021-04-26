local c = coroutine.wrap(function()
    coroutine.yield(10)
    coroutine.yield(20)
    coroutine.yield(30)
    coroutine.yield(40)
end)

return function()
    print(c())
    print(c())
    print(c())
    print(c())
end
