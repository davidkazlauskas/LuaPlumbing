
domainCtx = nil

initDomain = function(context)
    print("moo")
    print(context)
    domainCtx = context
    message("mainWnd",{"mwnd_insetprog","int"},{"",77})
    message("mainWnd",{"mwnd_insetlabel","string"},{"","YO SLICK"})
end

makePack = function(name,types,values)
    nat_registerPack(domainCtx,name,types,values)
end

message = function(name,types,values)
    nat_sendPack(domainCtx,name,types,values)
end
