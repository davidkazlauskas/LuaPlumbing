
domainCtx = nil

initDomain = function(context)
    print("moo")
    print(context)
    domainCtx = context
    makePack("sumpack",{"int","double"},{1,1.5})
end

makePack = function(name,types,values)
    registerPack(domainCtx,name,types,values)
end

