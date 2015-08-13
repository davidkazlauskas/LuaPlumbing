#ifndef MESSAGEABLE_24WTV8G9
#define MESSAGEABLE_24WTV8G9

#include <vector>
#include <memory>
#include <mutex>

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

struct MessageCache {

    void enqueue(const StrongPackPtr& pack) {
        Guard g(_mtx);
        _queue.push_back(pack);
    }

    template <class Func>
    void process(Func&& f) {
        std::vector< StrongPackPtr > steal;
        {
            Guard g(_mtx);
            if (0 == _queue.size()) {
                return;
            }
            steal = std::move(_queue);
        }

        for (auto& i: steal) {
            f(*i);
            i = nullptr;
        }
    }

    template <class Func>
    void processPtr(Func&& f) {
        std::vector< StrongPackPtr > steal;
        {
            Guard g(_mtx);
            if (0 == _queue.size()) {
                return;
            }
            steal = std::move(_queue);
        }

        for (auto& i: steal) {
            f(i);
            i = nullptr;
        }
    }

private:
    typedef std::lock_guard< std::mutex > Guard;
    std::mutex _mtx;
    std::vector< StrongPackPtr > _queue;
};

struct NotifierCache {

    void notify(templatious::VirtualPack& msg);
    void add(const std::shared_ptr< Messageable >& another);

private:
    typedef std::pair< bool, std::weak_ptr< Messageable > > PairType;
    std::vector< PairType  > _cache;
    typedef std::lock_guard< std::mutex > Guard;
    std::mutex _mtx;
};

#endif /* end of include guard: MESSAGEABLE_24WTV8G9 */
