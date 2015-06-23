
domainCtx = nil

initDomain = function(context)
    print("moo")
    print(context)
    domainCtx = context
    --message("mainWnd",{"mwnd_insetprog","int"},{"",77})
    --message("mainWnd",{"mwnd_insetlabel","string"},{"","YO SLICK"})
    message("mainWnd",VSig("mwnd_insetprog"),VInt(77))
    messageAsync(
        "mainWnd",
        function(newpack)
            print(newpack._1)
            print(newpack._2)
        end,
        VSig("mwnd_querylabel"),VString("washere")
    )

    nat_constructPack({
        values={
            some="nested",
            arbitrary={
                awesome="info",
                andSum="stuff"
            }
        },
        types={
            some="string",
            arbitrary={
                awesome="string",
                andSum="string"
            }
        },
    })
end

makePack = function(name,types,values)
    nat_registerPack(domainCtx,name,types,values)
end

function message(name, ...)
    local arguments = {...}
    local types,values = toTypeArrays(arguments)
    nat_sendPack(domainCtx,name,types,values)
end

messageAsync = function(name,callback,...)
    local arguments = {...}
    local types,values = toTypeArrays(arguments)
    nat_sendPackAsync(
        domainCtx,
        name,types,values,
        callback
    )
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

function toValueTree(tbl)
    arrVal = {}
    arrType = {}
    local iter = 1
    for _,iv in pairs(tbl) do
        for jk,jv in pairs(iv) do
            if (type(jv) == "table") then
                arrType[iter],arrVal[iter] = toValueTree(jv)
                arrVal[iter]
            else
                arrType[iter] = jk
                arrVal[iter] = jk
            end
            iter = iter + 1
        end
    end
end

function toValueTreeRec(types,values)

end

