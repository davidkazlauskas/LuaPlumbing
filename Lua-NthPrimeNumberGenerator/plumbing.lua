
luaContext = nil
require('mobdebug').start()

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
            self:message(messeagable,
                VSig("gen_inattachitself"),VMsg(named))
        end

    luaContext = context
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
    nat_registerPack(luaContext,name,types,values)
end

function registerCallback(name,func)
    nat_registerCallback(luaContext,name,func)
end

function messageAsync(name,callback,...)
    local tree = toValueTree(...)
    nat_sendPackAsync(
        luaContext,
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
    return {vmsg_raw=val}
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
    tbl = {...}
    return toValueTreeRec(tbl)
end

function isTrivialTable(tbl)
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
        if (type(iv) == "number") then
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
        elseif (isTrivialTable(iv)) then
            for jk,jv in pairs(iv) do
                arrType["_" .. iter] = jk
                arrVal["_" .. iter] = jv
                iter = iter + 1
            end
        else
            local vtree = toValueTreeRec(iv)
            arrType["_" .. iter] = vtree.types
            arrVal["_" .. iter] = vtree.values
            iter = iter + 1
        end
    end
    return {
        types=arrType,
        values=arrVal
    }
end

