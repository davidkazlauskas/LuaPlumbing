
luaContext = nil
require('mobdebug').start()

initLuaContext = function(context)
    local meta = getmetatable(context)
    meta.__index.message =
        function(self,messeagable,...)
            local vtree = toValueTree(...)
            nat_sendPack(self,messeagable,vtree)
        end

    meta.__index.messageWCallback =
        function(self,messeagable,callback,...)
            local vtree = toValueTree(...)
            nat_sendPackWCallback(self,messeagable,callback,vtree)
        end

    meta.__index.messageAsync =
        function(self,messeagable,...)
            local vtree = toValueTree(...)
            nat_sendPackAsync(self,messeagable,vtree)
        end

    meta.__index.messageAsyncWCallback =
        function(self,messeagable,callback,...)
            local vtree = toValueTree(...)
            nat_sendPackAsyncWCallback(self,messeagable,callback,vtree)
        end

    meta.__index.attachToProcessing =
        function(self,messeagable)
            local named = self:namedMesseagable("context")
            self:message(messeagable,
                VSig("gen_inattachitself"),VMsg(named))
        end

    luaContext = context
    --message("mainWnd",VSig("mwnd_insetprog"),VInt(77))
    --local mWnd = luaContext:namedMesseagable("mainWnd")
    --luaContext:message(mWnd,
        --VSig("mwnd_insetprog"),VInt(77))

    --luaContext:messageWCallback(mWnd,
        --function(result)
            --local valTree = result:values()
            --luaContext:message(mWnd,
                --VSig("mwnd_insetlabel"),VString(valTree._2))
        --end,
        --VSig("mwnd_querylabel"),VString("EMPTY"))

    --local myHandler = luaContext:makeLuaHandler(
        --function(mesg)
            --print(mesg:isST())
            --local tree = mesg:vtree()
            --local vals = tree:values()
            --print("ZE 2:" .. vals._1)
            ----luaContext:messageAsync(mWnd,
                ----VSig("mwnd_insetlabel"),VString("lolwut?"))
        --end)

    --luaContext:message(mWnd,
        --VSig("mwnd_inattachmsg"),VMsg(myHandler))

    --local ctxMessageable = luaContext:namedMesseagable("context")
    --luaContext:message(ctxMessageable,VSig("gen_inattachitself"),VMsg(mWnd))
    --luaContext:message(myHandler,VSig("gen_inattachitself"),VMsg(ctxMessageable))

    --luaContext:messageAsync(myHandler,VString("HEY"))

    --local conv = toValueTree(VSig("mwnd_insetprog"),VInt(77))
    --local tree = nat_testVTree(luaContext,conv)
    --local values = tree:values()
    --local types = tree:types()

    --luaContext:messageAsyncWCallback(mWnd,
        --function(out)
            --print("CALLED YO!")
        --end,
        --VSig("mwnd_insetlabel"),VString("MUAH BALLIN!"))

    local breaker = 7

    --messageAsync(
        --"mainWnd",
        --function(newpack)
            --local types = typeTree(newpack)
            --local values = valueTree(newpack)

            --printTree(types)
            --printTree(values)
        --end,
        --VSig("mwnd_querylabel"),VString("washere")
    --)

    --message("mainWnd",VSig("mwnd_inattachmsg"),VMsg(""))
    --registerCallback("mainWnd_outcallback",
        --function(msgHandle)
            --local types = typeTree(msgHandle)
            --local values = valueTree(msgHandle)

            --printTree(types)
            --printTree(values)
        --end
    --)

    --message("mainWnd",VSig("mwnd_inattachmsg"),VMsg("mainWnd_outcallback"))
    --message("service",VSig("gen_inattachitself"),VMsg("mainWnd"))

    --local outArr = toValueTree(
        --VSig("mwnd_querylabel"), VInt(32),
        --VString("someStr"),
        --VPack( VInt(7), VString("nested") )
    --)
    --printTree(outArr)
    --print(outArr.values[2])
    --print("C++ OUTPUT TYPES:")
    --printTree(outTypeTree)
    --print("C++ OUTPUT VALUES:")
    --printTree(outValueTree)
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
        if (isTrivialTable(iv)) then
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

