-- This code is derived from the SOM benchmarks, see AUTHORS.md file.
--
-- Copyright (c) 2016 Francois Perrad <francois.perrad@gadz.org>
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the 'Software'), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.

--[[
    The module 'bit' is available with:
      * LuaJIT
      * LuaBitOp extension which is available for:
          * Lua 5.1
          * Lua 5.2
    The module 'bit32' is available with:
      * Lua 5.2
      * Lua 5.3 when compiled with LUA_COMPAT_5_2
    The bitwise operators are added to Lua 5.3 as new lexemes (there causes
    lexical error in older version)
--]]
local band, bxor, rshift
if _VERSION < 'Lua 5.3' then
    local bit = bit32 or require'bit'
    band = bit.band
    bxor = bit.bxor
    rshift = bit.rshift
else
    band = assert(load'--[[band]] return function (a, b) return a & b end')()
    bxor = assert(load'--[[bxor]] return function (a, b) return a ~ b end')()
    rshift = assert(load'--[[rshift]] return function (a, b) return a >> b end')()
end

local alloc_array
local ok, table_new = pcall(require, 'table.new')       -- LuaJIT 2.1 extension
if ok then
    alloc_array = function (n)
        local t = table_new(n, 1)
        t.n = n
        return t
    end
else
    alloc_array = function (n)
        local t = {}
        t.n = n
        return t
    end
end

local Vector = {_CLASS = 'Vector'} do

local floor = math.floor

function Vector.new (size)
    local obj = {
        storage   = alloc_array(size or 50),
        first_idx = 1,
        last_idx  = 1,
    }
    return setmetatable(obj, {__index = Vector})
end

function Vector.with (elem)
    local v = Vector.new(1)
    v:append(elem)
    return v
end

function Vector:at (idx)
    if idx > self.storage.n then
        return nil
    end
    return self.storage[idx]
end

function Vector:at_put (idx, val)
    if idx > self.storage.n then
        local new_n = self.storage.n
        while idx > new_n do
            new_n = new_n * 2
        end

        local new_storage = alloc_array(new_n)
        for i = 1, self.storage.n do
            new_storage[i] = self.storage[i]
        end
        self.storage = new_storage
    end
    self.storage[idx] = val

    if self.last_idx < idx + 1 then
        self.last_idx = idx + 1
    end
end

function Vector:append (elem)
    if self.last_idx > self.storage.n then
        -- Need to expand capacity first
        local new_storage = alloc_array(2 * self.storage.n)
        for i = 1, self.storage.n do
            new_storage[i] = self.storage[i]
        end
        self.storage = new_storage
    end

    self.storage[self.last_idx] = elem
    self.last_idx = self.last_idx + 1
end

function Vector:is_empty ()
    return self.last_idx == self.first_idx
end

function Vector:each (fn)
    for i = self.first_idx, self.last_idx - 1 do
        fn(self.storage[i])
    end
end

function Vector:has_some (fn)
    for i = self.first_idx, self.last_idx - 1 do
        if fn(self.storage[i]) then
            return true
        end
    end
    return false
end

function Vector:get_one (fn)
    for i = self.first_idx, self.last_idx - 1 do
        local e = self.storage[i]
        if fn(e) then
            return e
        end
    end
    return nil
end

function Vector:remove_first ()
    if self:is_empty() then
        return nil
    end

    self.first_idx = self.first_idx + 1
    return self.storage[self.first_idx - 1]
end

function Vector:remove (obj)
    local new_array = alloc_array(self:capacity())
    local new_last = 1
    local found = false

    self:each(function (it)
        if it == obj then
            found = true
        else
            new_array[new_last] = it
            new_last = new_last + 1
        end
    end)

    self.storage   = new_array
    self.last_idx  = new_last
    self.first_idx = 1
    return found
end

function Vector:remove_all ()
    self.first_idx = 1
    self.last_idx = 1
    self.storage = alloc_array(self:capacity())
end

function Vector:size ()
    return self.last_idx - self.first_idx
end

function Vector:capacity ()
    return self.storage.n
end

function Vector:sort (fn)
    -- Make the argument, block, be the criterion for ordering elements of
    -- the receiver.
    -- Sort blocks with side effects may not work right.
    if self:size() > 0 then
        self:sort_range(self.first_idx, self.last_idx - 1, fn)
    end
end

function Vector:sort_range (i, j, fn)
    assert(fn)

    -- The prefix d means the data at that index.
    local n = j + 1 - i
    if n <= 1 then
        -- Nothing to sort
        return
    end

    local storage = self.storage
    -- Sort di, dj
    local di = storage[i]
    local dj = storage[j]

    -- i.e., should di precede dj?
    if not fn(di, dj) then
        local tmp = storage[i]
        storage[i] = storage[j]
        storage[j] = tmp
        local tt = di
        di = dj
        dj = tt
    end

    -- NOTE: For DeltaBlue, this is never reached.
    if n > 2 then               -- More than two elements.
        local ij  = floor((i + j) / 2)  -- ij is the midpoint of i and j.
        local dij = storage[ij]         -- Sort di,dij,dj.  Make dij be their median.

        if fn(di, dij) then             -- i.e. should di precede dij?
            if not fn(dij, dj) then     -- i.e., should dij precede dj?
               local tmp = storage[j]
               storage[j] = storage[ij]
               storage[ij] = tmp
               dij = dj
            end
        else                            -- i.e. di should come after dij
            local tmp = storage[i]
            storage[i] = storage[ij]
            storage[ij] = tmp
            dij = di
        end

        if n > 3 then           -- More than three elements.
            -- Find k>i and l<j such that dk,dij,dl are in reverse order.
            -- Swap k and l.  Repeat this procedure until k and l pass each other.
            local k = i
            local l = j - 1

            while true do
                -- i.e. while dl succeeds dij
                while k <= l and fn(dij, storage[l]) do
                    l = l - 1
                end

                k = k + 1
                -- i.e. while dij succeeds dk
                while k <= l and fn(storage[k], dij) do
                    k = k + 1
                end

                if k > l then
                    break
                end

                local tmp = storage[k]
                storage[k] = storage[l]
                storage[l] = tmp
            end

            -- Now l < k (either 1 or 2 less), and di through dl are all
            -- less than or equal to dk through dj.  Sort those two segments.
            self:sort_range(i, l, fn)
            self:sort_range(k, j, fn)
        end
    end
end

end -- class Vector

local Set = {_CLASS = 'Set'} do

local INITIAL_SIZE = 10

function Set.new (size)
    local obj = {
        items = Vector.new(size or INITIAL_SIZE)
    }
    return setmetatable(obj, {__index = Set})
end

function Set:size ()
    return self.items:size()
end

function Set:each (fn)
    self.items:each(fn)
end

function Set:has_some (fn)
    return self.items:has_some(fn)
end

function Set:get_one (fn)
    return self.items:get_one(fn)
end

function Set:add (obj)
    if not self:contains(obj) then
        self.items:append(obj)
    end
end

function Set:remove_all ()
    self.items:remove_all()
end

function Set:collect (fn)
    local coll = Vector.new()
    self:each(function (it)
        coll:append(fn(it))
    end)
    return coll
end

function Set:contains (obj)
    return self:has_some(function (it) return it == obj end)
end

end -- class Set

local IdentitySet = {_CLASS = 'IdentitySet'} do
setmetatable(IdentitySet, {__index = Set})

function IdentitySet.new (size)
    local obj = Set.new(size)
    return setmetatable(obj, {__index = IdentitySet})
end

function IdentitySet:contains (obj)
    return self:has_some(function (it) return it == obj end)
end

end -- class IdentitySet

local Entry = {_CLASS = 'Entry'} do

function Entry.new (hash, key, value, next)
    local obj = {
        hash  = hash,
        key   = key,
        value = value,
        next  = next,
    }
    return setmetatable(obj, {__index = Entry})
end

function Entry:match (hash, key)
    return self.hash == hash and self.key == key
end

end -- class Entry

local Dictionary = {_CLASS = 'Dictionary'} do

local INITIAL_CAPACITY = 16

function Dictionary.new (size)
    local obj = {
        buckets = alloc_array(size or INITIAL_CAPACITY),
        size    = 0,
    }
    return setmetatable(obj, {__index = Dictionary})
end

function Dictionary:hash (key)
    if not key then
        return 0
    end
    local hash = key:custom_hash()
    return bxor(hash, rshift(hash, 16))
end

function Dictionary:is_empty ()
    return self.size == 0
end

function Dictionary:get_bucket_idx (hash)
    return band(self.buckets.n - 1, hash) + 1
end

function Dictionary:get_bucket (hash)
    return self.buckets[self:get_bucket_idx(hash)]
end

function Dictionary:at (key)
    local hash = self:hash(key)
    local e = self:get_bucket(hash)

    while e do
        if e:match(hash, key) then
            return e.value
        end
        e = e.next
    end
    return nil
end

function Dictionary:contains_key (key)
    local hash = self:hash(key)
    local e = self:get_bucket(hash)

    while e do
        if e.match(hash, key) then
            return true
        end
        e = e.next
    end
    return false
end

function Dictionary:at_put (key, value)
    local hash = self:hash(key)
    local i = self:get_bucket_idx(hash)
    local current = self.buckets[i]

    if not current then
        self.buckets[i] = self:new_entry(key, value, hash)
        self.size = self.size + 1
    else
        self:insert_bucket_entry(key, value, hash, current)
    end

    if self.size > self.buckets.n then
        self:resize()
    end
end

function Dictionary:new_entry (key, value, hash)
    return Entry.new(hash, key, value, nil)
end

function Dictionary:insert_bucket_entry (key, value, hash, head)
    local current = head

    while true do
        if current:match(hash, key) then
            current.value = value
            return
        end
        if not current.next then
            self.size = self.size + 1
            current.next = self:new_entry(key, value, hash)
            return
        end
        current = current.next
    end
end

function Dictionary:resize ()
    local old_storage = self.buckets
    self.buckets = alloc_array(old_storage.n * 2)
    self:transfer_entries(old_storage)
end

function Dictionary:transfer_entries (old_storage)
    local buckets = self.buckets
    for i = 1, old_storage.n do
        local current = old_storage[i]

        if current then
            old_storage[i] = nil
            if not current.next then
                local hash = band(current.hash, buckets.n - 1) + 1
                buckets[hash] = current
            else
                self:split_bucket(old_storage, i, current)
            end
        end
    end
end

function Dictionary:split_bucket (old_storage, i, head)
    local lo_head, lo_tail = nil, nil
    local hi_head, hi_tail = nil, nil
    local current = head

    while current do
        if band(current.hash, old_storage.n) == 0 then
            if not lo_tail then
               lo_head = current
            else
                lo_tail.next = current
            end
            lo_tail = current
        else
            if not hi_tail then
                hi_head = current
            else
                hi_tail.next = current
            end
            hi_tail = current
        end
       current = current.next
    end

    if lo_tail then
        lo_tail.next = nil
        self.buckets[i] = lo_head
    end
    if hi_tail then
        hi_tail.next = nil
        self.buckets[i + old_storage.n] = hi_head
    end
end

function Dictionary:remove_all ()
    self.buckets = alloc_array(self.buckets.n)
    self.size = 0
end

function Dictionary:keys ()
    local keys = Vector.new(self.size)
    local buckets = self.buckets
    for i = 1, buckets.n do
        local current = buckets[i]
        while current do
            keys:append(current.key)
            current = current.next
        end
    end
    return keys
end

function Dictionary:values ()
    local vals = Vector.new(self.size)
    local buckets = self.buckets
    for i = 1, buckets.n do
        local current = buckets[i]
        while current do
            vals:append(current.value)
            current = current.next
        end
    end
    return vals
end

end -- class Dictionary

local IdEntry = {_CLASS = 'IdEntry'} do
setmetatable(IdEntry, {__index = Entry})

function IdEntry.new (hash, key, value, next)
    local obj = Entry.new (hash, key, value, next)
    return setmetatable(obj, {__index = IdEntry})
end

function IdEntry:match (hash, key)
    return self.hash == hash and self.key == key
end

end -- class IdEntry

local IdentityDictionary = {_CLASS = 'IdentityDictionary'} do
setmetatable(IdentityDictionary, {__index = Dictionary})

function IdentityDictionary.new (size)
    local obj = Dictionary.new (size)
    return setmetatable(obj, {__index = Dictionary})
end

function IdentityDictionary:new_entry (key, value, hash)
    return IdEntry.new(hash, key, value, nil)
end

end -- class IdentityDictionary

local Random = {_CLASS = 'Random'} do

function Random.new ()
    local obj = {seed = 74755}
    return setmetatable(obj, {__index = Random})
end

function Random:next ()
  self.seed = band(((self.seed * 1309) + 13849), 65535);
  return self.seed;
end

end -- class Random

---------------------------------

local benchmark = {} do

function benchmark:inner_benchmark_loop (inner_iterations)
    for _ = 1, inner_iterations do
        if not self:verify_result(self:benchmark()) then
            return false
        end
    end
    return true
end

function benchmark:benchmark ()
    error 'subclass_responsibility'
end

function benchmark:verify_result ()
    error 'subclass_responsibility'
end

end

---------------------------------

local NO_TASK = nil
local NO_WORK = nil

local IDLER     = 1
local WORKER    = 2
local HANDLER_A = 3
local HANDLER_B = 4
local DEVICE_A  = 5
local DEVICE_B  = 6

local TRACING = false

local RBObject = {_CLASS = 'RBObject'} do

function RBObject:append (packet, queue_head)
    packet.link = NO_WORK
    if NO_WORK == queue_head then
        return packet
    end

    local mouse = queue_head

    local link = mouse.link
    while NO_WORK ~= link do
        mouse = link
        link = mouse.link
    end

    mouse.link = packet
    return queue_head
end

end -- abstract RBObject

local DeviceTaskDataRecord = {_CLASS = 'DeviceTaskDataRecord'} do
setmetatable(DeviceTaskDataRecord, {__index = RBObject})

function DeviceTaskDataRecord.new ()
    local obj = {
        pending = NO_WORK,
    }
    return setmetatable(obj, {__index = DeviceTaskDataRecord})
end

end -- class DeviceTaskDataRecord

local HandlerTaskDataRecord = {_CLASS = 'HandlerTaskDataRecord'} do
setmetatable(HandlerTaskDataRecord, {__index = RBObject})

function HandlerTaskDataRecord.new ()
    local obj = {
        work_in   = NO_WORK,
        device_in = NO_WORK,
    }
    return setmetatable(obj, {__index = HandlerTaskDataRecord})
end

function HandlerTaskDataRecord:device_in_add (packet)
    self.device_in = self:append(packet, self.device_in)
end

function HandlerTaskDataRecord:work_in_add (packet)
    self.work_in = self:append(packet, self.work_in)
end

end -- class HandlerTaskDataRecord

local IdleTaskDataRecord = {_CLASS = 'IdleTaskDataRecord'} do
setmetatable(IdleTaskDataRecord, {__index = RBObject})

function IdleTaskDataRecord.new ()
    local obj = {
        control = 1,
        count   = 10000,
    }
    return setmetatable(obj, {__index = IdleTaskDataRecord})
end

end -- class IdleTaskDataRecord

local WorkerTaskDataRecord = {_CLASS = 'WorkerTaskDataRecord'} do
setmetatable(WorkerTaskDataRecord, {__index = RBObject})

function WorkerTaskDataRecord.new ()
    local obj = {
        destination = HANDLER_A,
        count = 0,
    }
    return setmetatable(obj, {__index = WorkerTaskDataRecord})
end

end -- class WorkerTaskDataRecord

local Packet = {_CLASS = 'Packet'} do
setmetatable(Packet, {__index = RBObject})

function Packet.new (link, identity, kind)
    local obj = {
        link     = link,
        kind     = kind,
        identity = identity,
        datum    = 1,
        data     = {0, 0, 0, 0},
    }
    return setmetatable(obj, {__index = Packet})
end

end -- class Packet

local TaskState = {_CLASS = 'TaskState'} do
setmetatable(TaskState, {__index = RBObject})

function TaskState.new ()
    local obj = {
        task_holding  = false,
        task_waiting  = false,
        packt_pending = false,
    }
    return setmetatable(obj, {__index = TaskState})
end

function TaskState:is_packet_pending ()
    return self.packt_pending
end

function TaskState:is_task_waiting ()
    return self.task_waiting
end

function TaskState:is_task_holding ()
    return self.task_holding
end

function TaskState:packet_pending ()
    self.packt_pending = true
    self.task_waiting  = false
    self.task_holding  = false
    return self
end

function TaskState:running ()
    self.packt_pending = false
    self.task_waiting  = false
    self.task_holding  = false
    return self
end

function TaskState:waiting ()
    self.packt_pending = false
    self.task_holding  = false
    self.task_waiting  = true
    return self
end

function TaskState:waiting_with_packet ()
    self.task_holding  = false
    self.task_waiting  = true
    self.packt_pending = true
    return self
end

function TaskState:is_running ()
    return not self.packt_pending and not self.task_waiting and not self.task_holding
end

function TaskState:is_task_holding_or_waiting ()
    return self.task_holding or (not self.packt_pending and self.task_waiting)
end

function TaskState:is_waiting ()
    return not self.packt_pending and self.task_waiting and not self.task_holding
end

function TaskState:is_waiting_with_packet ()
    return self.packt_pending and self.task_waiting and not self.task_holding
end

function TaskState.create_packet_pending ()
    return TaskState.new():packet_pending()
end

function TaskState.create_running ()
    return TaskState.new():running()
end

function TaskState.create_waiting ()
    return TaskState.new():waiting()
end

function TaskState.create_waiting_with_packet ()
    return TaskState.new():waiting_with_packet()
end

end -- class TaskState

local TaskControlBlock = {_CLASS = 'TaskControlBlock'} do
setmetatable(TaskControlBlock, {__index = TaskState})

function TaskControlBlock.new (link, identity, priority, initial_work_queue,
                               initial_state, private_data, fn)
    local obj = {
        link = link,
        identity = identity,
        priority = priority,
        input = initial_work_queue,
        handle = private_data,

        packt_pending = initial_state:is_packet_pending(),
        task_waiting  = initial_state:is_task_waiting(),
        task_holding  = initial_state:is_task_holding(),

        fn = fn,
    }
    return setmetatable(obj, {__index = TaskControlBlock})
end

function TaskControlBlock:add_input_and_check_priority (packet, old_task)
    if NO_WORK == self.input then
        self.input = packet
        self.packt_pending = true
        if self.priority > old_task.priority then
            return self
        end
    else
        self.input = self:append(packet, self.input)
    end
    return old_task
end

function TaskControlBlock:run_task ()
    local message
    if self:is_waiting_with_packet() then
        message = self.input
        self.input = message.link
        if NO_WORK == self.input then
            self:running()
        else
            self:packet_pending()
        end
    else
        message = NO_WORK
    end
    return self.fn(message, self.handle)
end

end -- class TaskControlBlock

local Scheduler = {_CLASS = 'Scheduler'} do
setmetatable(Scheduler, {__index = RBObject})

local DEVICE_PACKET_KIND = 0
local WORK_PACKET_KIND   = 1

local DATA_SIZE = 4

function Scheduler.new ()
    local obj = {
        -- init tracing
        layout = 0,

        -- init scheduler
        task_list    = NO_TASK,
        current_task = NO_TASK,
        current_task_identity = 0,
        task_table = {NO_TASK, NO_TASK, NO_TASK, NO_TASK, NO_TASK, NO_TASK},

        queue_count = 0,
        hold_count  = 0,
    }
    return setmetatable(obj, {__index = Scheduler})
end

function Scheduler:create_device (identity, priority, work, state)
    self:create_task(identity, priority, work, state,
                     DeviceTaskDataRecord.new(),
                     function (packet, data)
        if NO_WORK == packet then
            packet = data.pending
            if NO_WORK == packet then
                return self:wait()
            else
                data.pending = NO_WORK
                return self:queue_packet(packet)
            end
        else
            data.pending = packet
            if TRACING then
                self:trace(packet.datum)
            end
            return self:hold_self()
        end
    end)
end

function Scheduler:create_handler (identity, priority, work, state)
    self:create_task(identity, priority, work, state,
                     HandlerTaskDataRecord.new(),
                     function (packet, data)
        if NO_WORK ~= packet then
            if WORK_PACKET_KIND == packet.kind then
                data:work_in_add(packet)
            else
                data:device_in_add(packet)
            end
        end

        local work_packet = data.work_in
        if NO_WORK == work_packet then
            return self:wait()
        else
            local count = work_packet.datum
            if count > DATA_SIZE then
                data.work_in = work_packet.link
                return self:queue_packet(work_packet)
            else
                local device_packet = data.device_in
                if NO_WORK == device_packet then
                    return self:wait()
                else
                    data.device_in = device_packet.link
                    device_packet.datum = work_packet.data[count]
                    work_packet.datum = count + 1
                    return self:queue_packet(device_packet)
                end
            end
        end
    end)
end

function Scheduler:create_idler (identity, priority, work, state)
    self:create_task(identity, priority, work, state,
                     IdleTaskDataRecord.new(),
                     function (_, data)
        data.count = data.count - 1
        if 0 == data.count then
            return self:hold_self()
        else
            if 0 == data.control & 1 then
                data.control = data.control / 2
                return self:release(DEVICE_A)
            else
                data.control = ((data.control - 1) / 2) ~ 53256
                return self:release(DEVICE_B)
            end
        end
    end)
end

function Scheduler:create_packet (link, identity, kind)
    return Packet.new(link, identity, kind)
end

function Scheduler:create_task (identity, priority, work, state, data, fn)
    local tcb = TaskControlBlock.new(self.task_list, identity, priority,
                                     work, state, data, fn)
    self.task_list = tcb
    self.task_table[identity] = tcb
end

function Scheduler:create_worker (identity, priority, work, state)
    self:create_task(identity, priority, work, state,
                     WorkerTaskDataRecord.new(),
                     function (packet, data)
        if NO_WORK == packet then
            return self:wait()
        else
            data.destination = (HANDLER_A == data.destination) and HANDLER_B or HANDLER_A
            packet.identity = data.destination
            packet.datum = 1
            for i = 1, DATA_SIZE do
                data.count = data.count + 1
                if data.count > 26 then
                   data.count = 1
                end
                packet.data[i] = 65 + data.count - 1
            end
            return self:queue_packet(packet)
        end
    end)
end

function Scheduler:start ()
    local queue
    self:create_idler(IDLER, 0, NO_WORK, TaskState.create_running())
    queue = self:create_packet(NO_WORK, WORKER, WORK_PACKET_KIND)
    queue = self:create_packet(queue,   WORKER, WORK_PACKET_KIND)

    self:create_worker(WORKER, 1000, queue, TaskState.create_waiting_with_packet())
    queue = self:create_packet(NO_WORK, DEVICE_A, DEVICE_PACKET_KIND)
    queue = self:create_packet(queue,   DEVICE_A, DEVICE_PACKET_KIND)
    queue = self:create_packet(queue,   DEVICE_A, DEVICE_PACKET_KIND)

    self:create_handler(HANDLER_A, 2000, queue, TaskState.create_waiting_with_packet())
    queue = self:create_packet(NO_WORK, DEVICE_B, DEVICE_PACKET_KIND)
    queue = self:create_packet(queue,   DEVICE_B, DEVICE_PACKET_KIND)
    queue = self:create_packet(queue,   DEVICE_B, DEVICE_PACKET_KIND)

    self:create_handler(HANDLER_B, 3000, queue, TaskState.create_waiting_with_packet())
    self:create_device(DEVICE_A, 4000, NO_WORK, TaskState.create_waiting())
    self:create_device(DEVICE_B, 5000, NO_WORK, TaskState.create_waiting())

    self:schedule()

    return self.queue_count == 23246 and self.hold_count == 9297
end

function Scheduler:find_task (identity)
    local task = self.task_table[identity]
    assert(task ~= NO_TASK, 'find_task failed')
    return task
end

function Scheduler:hold_self ()
    self.hold_count = self.hold_count + 1
    local current_task = self.current_task
    current_task.task_holding = true
    return current_task.link
end

function Scheduler:queue_packet (packet)
    local task = self:find_task(packet.identity)
    if NO_TASK == task then
        return NO_TASK
    end

    self.queue_count = self.queue_count + 1

    packet.link     = NO_WORK
    packet.identity = self.current_task_identity
    return task:add_input_and_check_priority(packet, self.current_task)
end

function Scheduler:release (identity)
    local task = self:find_task(identity)
    if NO_TASK == task then
        return NO_TASK
    end

    task.task_holding = false

    if task.priority > self.current_task.priority then
        return task
    else
        return self.current_task
    end
end

function Scheduler:trace (id)
    self.layout = self.layout - 1
    if 0 >= self.layout then
        io.stdout:write'\n'
        self.layout = 50
    end
    io.stdout:write(tostring(id))
end

function Scheduler:wait ()
    local current_task = self.current_task
    current_task.task_waiting = true
    return current_task
end

function Scheduler:schedule ()
    self.current_task = self.task_list
    while self.current_task ~= NO_TASK do
        if self.current_task:is_task_holding_or_waiting() then
            self.current_task = self.current_task.link
        else
            self.current_task_identity = self.current_task.identity
            if TRACING then
                self:trace(self.current_task_identity - 1)
            end
            self.current_task = self.current_task:run_task()
        end
    end
end

end -- class Scheduler

local richards = {} do
setmetatable(richards, {__index = benchmark})

function richards:benchmark ()
    return Scheduler.new():start()
end

function richards:verify_result (result)
    return result
end

end -- object richards

return function(N)
    richards:inner_benchmark_loop(N)
end
