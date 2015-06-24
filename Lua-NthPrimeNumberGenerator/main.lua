
domainCtx = nil
require('mobdebug').start()

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

    --nat_constructPack({
        --values={
            --some="nested",
            --arbitrary={
                --awesome="info",
                --andSum="stuff"
            --}
        --},
        --types={
            --some="string",
            --arbitrary={
                --awesome="string",
                --andSum="string"
            --}
        --},
    --})
    local outArr = toValueTree(
        VSig("mwnd_querylabel"), VInt(32),
        VString("someStr"),
        VPack( VInt(7), VString("nested") )
    )
    printTree(outArr)
    print(outArr.values[2])
    local outTypeTree,outValueTree = nat_constructPack(outArr)
    print("C++ OUTPUT TYPES:")
    printTree(outTypeTree)
    print("C++ OUTPUT VALUES:")
    printTree(outValueTree)
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

