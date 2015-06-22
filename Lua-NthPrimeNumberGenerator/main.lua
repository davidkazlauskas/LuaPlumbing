
domainCtx = nil

initDomain = function(context)
    print("moo")
    print(context)
    domainCtx = context
    message("mainWnd",{"mwnd_insetprog","int"},{"",77})
    message("mainWnd",{"mwnd_insetlabel","string"},{"","YO SLICK"})
    messageAsync(
        function(newpack)
            print("BRONTO")
            print(newpack._1)
            print(newpack._2)
            print("SAURUS")
        end,
        "mainWnd",{"mwnd_querylabel","string"},{"","washere"}
    )
end

makePack = function(name,types,values)
    nat_registerPack(domainCtx,name,types,values)
end

message = function(name,types,values)
    nat_sendPack(domainCtx,name,types,values)
end

messageAsync = function(callback,name,types,values)
    nat_sendPackAsync(
        domainCtx,
        name,types,values,
        callback
    )
end

VInt = function(value)
    return {int=value}
end

VDouble = function(value)
    return {double=value}
end

VString = function(value)
    return {string=value}
end

VSig = function(value)
    return {value=""}
end

function toTypeArrays(...)
    arrVal = {}
    arrType = {}
    local iter = 1
    for k,v in ipairs(arg) do
        arrType[iter] = k
        arrVal[iter] = v
        iter = iter + 1
    end

    return arrType, arrVal
end
