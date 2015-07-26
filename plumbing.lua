
__luaContext = nil
--require('mobdebug').start()

function luaContext()
    return __luaContext
end

initLuaContext = function(context)
    local meta = getmetatable(context)
    meta.__index.message =
        function(self,messeagable,...)
            local vtree = toValueTree(...)
            return nat_sendPack(self,messeagable,vtree)
        end

    meta.__index.messageWCallback =
        function(self,messeagable,callback,...)
            local vtree = toValueTree(...)
            return nat_sendPackWCallback(self,messeagable,callback,vtree)
        end

    meta.__index.messageRetValues =
        function(self,messeagable,...)
            local outVal = nil
            local callback = function(out) outVal = out:values() end
            local vtree = toValueTree(...)
            local didCall = nat_sendPackWCallback(self,messeagable,callback,vtree)
            assert( didCall )
            return outVal
        end

    meta.__index.messageAsync =
        function(self,messeagable,...)
            local vtree = toValueTree(...)
            nat_sendPackAsync(self,messeagable,nil,vtree)
        end

    meta.__index.messageAsyncWError =
        function(self,messeagable,errorcallback,...)
            local vtree = toValueTree(...)
            nat_sendPackAsync(self,messeagable,errorcallback,vtree)
        end

    meta.__index.messageAsyncWCallback =
        function(self,messeagable,callback,...)
            local vtree = toValueTree(...)
            nat_sendPackAsyncWCallback(self,messeagable,callback,nil,vtree)
        end

    meta.__index.messageAsyncWCallbackWError =
        function(self,messeagable,callback,errorcallback,...)
            local vtree = toValueTree(...)
            nat_sendPackAsyncWCallback(self,messeagable,callback,errorcallback,vtree)
        end

    meta.__index.attachToProcessing =
        function(self,messeagable)
            local named = self:namedMesseagable("context")
            return self:message(messeagable,
                VSig("gen_inattachitself"),VMsg(named))
        end

    meta.__index.attachContextTo =
        function(self,messeagable)
            local named = self:namedMesseagable("context")
            return self:message(named,
                VSig("gen_inattachitself"),VMsg(messeagable))
        end

    meta.__index.makeLuaMatchHandler =
        function(self,...)
            local match = VMatchFunctor.create(...)
            local handler = function(pack)
                return match:tryMatch(pack)
            end
            local named = self:namedMesseagable("context")
            self:attachToProcessing(handler,
                VSig("gen_inattachitself"),VMsg(named))
            return self:makeLuaHandler(handler)
        end

    __luaContext = context
end

function printTree(tree)
    printTreeRec(tree,2)
end

function printTreeRec(tree,idx)
    local padding = ""
    for i = 1,idx do
        padding = padding .. " "
    end
    for k,v in pairs(tree) do
        if (type(v) ~= "table") then
            print( padding .. k .. " -> " .. v )
        else
            print( padding .. k .. " : " )
            printTreeRec(v,idx+2)
        end
    end
end

function makePack(name,types,values)
    nat_registerPack(luaContext(),name,types,values)
end

function registerCallback(name,func)
    nat_registerCallback(luaContext(),name,func)
end

function messageAsync(name,callback,...)
    local tree = toValueTree(...)
    nat_sendPackAsync(
        luaContext(),
        name,tree,
        callback
    )
end

function valueTree(theMessage)
    return nat_getValueTree(theMessage)
end

function typeTree(theMessage)
    return nat_getTypeTree(theMessage)
end

function VInt(value)
    return {int=value}
end

function VDouble(value)
    return {double=value}
end

function VBool(value)
    return {bool=value}
end

function VString(value)
    return {string=value}
end

function VSig(value)
    result = {}
    result[value] = ""
    return result
end

function VPack(...)
    return {...}
end

function VMsg(val)
    if (type(val) == "string") then
        return {vmsg_name=val}
    end
    return {vmsg_raw_strong=val}
end

function toTypeArrays(tbl)
    arrVal = {}
    arrType = {}
    local iter = 1
    for _,iv in pairs(tbl) do
        for jk,jv in pairs(iv) do
            arrType[iter] = jk
            arrVal[iter] = jv
            iter = iter + 1
        end
    end
    return arrType, arrVal
end

function toValueTree(...)
    local tbl = {...}
    return toValueTreeRec(tbl)
end

function isTrivialTable(tbl)
    if (type(tbl) ~= "table") then
        return false
    end
    local count = 0

    for k,v in pairs(tbl) do
        if (type(v) == "table") then
            return false
        end
        count = count + 1
    end

    if (count == 1) then
        return true;
    end

    return false
end

function toValueTreeRec(tbl)
    local arrType = {}
    local arrVal = {}
    local iter = 1
    for ik,iv in pairs(tbl) do
        if (isTrivialTable(iv)) then
            for jk,jv in pairs(iv) do
                arrType["_" .. iter] = jk
                arrVal["_" .. iter] = jv
                iter = iter + 1
            end
        elseif (type(iv) == "number") then
            arrType["_" .. iter] = "double"
            arrVal["_" .. iter] = iv
            iter = iter + 1
        elseif (type(iv) == "string") then
            arrType["_" .. iter] = "string"
            arrVal["_" .. iter] = iv
            iter = iter + 1
        elseif (type(iv) == "boolean") then
            arrType["_" .. iter] = "bool"
            arrVal["_" .. iter] = iv
            iter = iter + 1
        elseif (nat_isMesseagable(iv)) then
            arrType["_" .. iter] = "vmsg_raw_strong"
            arrVal["_" .. iter] = iv
            iter = iter + 1
        elseif (type(iv) == "table") then
            local vtree = toValueTreeRec(iv)
            arrType["_" .. iter] = vtree.types
            arrVal["_" .. iter] = vtree.values
            iter = iter + 1
        else
            assert( false, "Unknown type in message signature." )
        end
    end
    return {
        types=arrType,
        values=arrVal
    }
end

function VMatch(funct,...)
    local sig = {...}
    return {
        func=funct,
        signature=sig
    }
end

VMatchFunctor = {}
VMatchFunctor.__index = VMatchFunctor

function VMatchFunctor.create(...)
    local vmf = {}
    local matches = {...}
    setmetatable(vmf,VMatchFunctor)
    vmf.matches = matches
    return vmf
end

function VMatchFunctor:getFunction(vTypeTree)
    for _,i in ipairs(self.matches) do
        local idx = 1
        local matched = true
        for _,j in ipairs(i.signature) do
            if (j ~= vTypeTree["_" .. idx]) then
                matched = false
                break
            end
            idx = idx + 1
        end
        if (matched) then
            return i.func
        end
    end

    return nil
end

function VMatchFunctor:tryMatch(inPack)
    local vtree = inPack:vtree()
    local typeTree = vtree:types()
    local func = self:getFunction(typeTree)
    if (func ~= nil) then
        func(inPack,vtree)
        return true
    end

    return false
end

