
domainCtx = nil

initDomain = function(context)
    print("moo")
    print(context)
    domainCtx = context
end

makePack = function(name,size,types,values)
    registerPack(name,size,types,values)
end

makePack("sumpack",3,{"int","double"},{"1",1.5})

