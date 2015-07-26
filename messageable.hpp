#ifndef MESSAGEABLE_24WTV8G9
#define MESSAGEABLE_24WTV8G9

namespace templatious {
    struct DynVPackFactory;
    struct DynVPackFactoryBuilder;
    struct VirtualMatchFunctor;
    struct VirtualPack;
}

class Messageable {
public:
    // this is for sending message across threads
    virtual void message(const std::shared_ptr< templatious::VirtualPack >& msg) = 0;

    // this is for sending stack allocated (faster)
    // if we know we're on the same thread as GUI
    virtual void message(templatious::VirtualPack& msg) = 0;
};

typedef std::shared_ptr< Messageable > StrongMsgPtr;
typedef std::weak_ptr< Messageable > WeakMsgPtr;
typedef std::shared_ptr<
    templatious::VirtualPack > StrongPackPtr;

#endif /* end of include guard: MESSAGEABLE_24WTV8G9 */
