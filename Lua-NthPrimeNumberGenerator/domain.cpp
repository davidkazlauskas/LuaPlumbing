// All the domain logic is here.
// Notice, that while not knowing what
// Qt is or including single header of
// Qt we're able to fully manipulate
// the GUI as long as we specify the messages
// and GUI side implements them.

#include <templatious/FullPack.hpp>
#include <templatious/detail/DynamicPackCreator.hpp>

#include "domain.h"
#include "mainwindow_interface.h"

TEMPLATIOUS_TRIPLET_STD;

namespace {

    typedef std::weak_ptr< LuaContext > WeakCtxPtr;

    void writePtrToString(const void* ptr,std::string& out) {
        out.clear();
        TEMPLATIOUS_REPEAT(sizeof(ptr)) {
            out += '7';
        }
        out += '3';
        const char* ser = reinterpret_cast<const char*>(&ptr);
        TEMPLATIOUS_0_TO_N(i,sizeof(ptr)) {
            out[i] = ser[i];
        }
    }

    void* ptrFromString(const std::string& out) {
        void* result = nullptr;
        memcpy(&result,out.data(),sizeof(result));
        return result;
    }

    typedef templatious::TypeNodeFactory TNF;

    auto intNode = TNF::makePodNode<int>(
        [](void* ptr,const char* arg) {
            int* asInt = reinterpret_cast<int*>(ptr);
            new (ptr) int(std::atoi(arg));
        },
        [](const void* ptr,std::string& out) {
            out = std::to_string(
                *reinterpret_cast<const int*>(ptr));
        }
    );

    auto doubleNode = TNF::makePodNode<double>(
        [](void* ptr,const char* arg) {
            double* asInt = reinterpret_cast<double*>(ptr);
            new (ptr) double(std::atof(arg));
        },
        [](const void* ptr,std::string& out) {
            out = std::to_string(
                *reinterpret_cast<const double*>(ptr));
        }
    );

    auto stringNode = TNF::makeFullNode<std::string>(
        [](void* ptr,const char* arg) {
            new (ptr) std::string(arg);
        },
        [](void* ptr) {
            std::string* sptr = reinterpret_cast<std::string*>(ptr);
            sptr->~basic_string();
        },
        [](const void* ptr,std::string& out) {
            const std::string* sptr =
                reinterpret_cast<const std::string*>(ptr);
            out.assign(*sptr);
        }
    );

    typedef std::shared_ptr< templatious::VirtualPack > VPackPtr;

    auto vpackNode = TNF::makeFullNode<VPackPtr>(
        // here, we assume we receive pointer
        // to exact copy of the pack
        [](void* ptr,const char* arg) {
            new (ptr) VPackPtr(
                *reinterpret_cast<const VPackPtr*>(arg)
            );
        },
        [](void* ptr) {
            VPackPtr* vpPtr = reinterpret_cast<VPackPtr*>(ptr);
            vpPtr->~shared_ptr();
        },
        [](const void* ptr,std::string& out) {
            // write pointer
            writePtrToString(ptr,out);
        }
    );

    auto messeagableWeakNode = TNF::makeFullNode< WeakMsgPtr >(
        [](void* ptr,const char* arg) {
            new (ptr) WeakMsgPtr(
                *reinterpret_cast<const WeakMsgPtr*>(arg)
            );
        },
        [](void* ptr) {
            WeakMsgPtr* msPtr = reinterpret_cast<WeakMsgPtr*>(ptr);
            msPtr->~weak_ptr();
        },
        [](const void* ptr,std::string& out) {
            writePtrToString(ptr,out);
        }
    );

#define ATTACH_NAMED_DUMMY(factory,name,type)   \
    factory.attachNode(name,TNF::makeDummyNode< type >(name))

    templatious::DynVPackFactory buildTypeIndex() {

        templatious::DynVPackFactoryBuilder bld;
        bld.attachNode("int",intNode);
        bld.attachNode("double",doubleNode);
        bld.attachNode("string",stringNode);
        bld.attachNode("vpack",vpackNode);
        bld.attachNode("vmsg",messeagableWeakNode);

        typedef MainWindowInterface MWI;
        typedef GenericMesseagableInterface GNI;
        ATTACH_NAMED_DUMMY( bld, "mwnd_insetprog", MWI::InSetProgress );
        ATTACH_NAMED_DUMMY( bld, "mwnd_insetlabel", MWI::InSetStatusText );
        ATTACH_NAMED_DUMMY( bld, "mwnd_querylabel", MWI::QueryLabelText );
        ATTACH_NAMED_DUMMY( bld, "mwnd_inattachmsg", MWI::InAttachMesseagable );
        ATTACH_NAMED_DUMMY( bld, "gen_inattachitself", GNI::AttachItselfToMesseagable );

        return bld.getFactory();
    }
}

static auto vFactory = buildTypeIndex();

void initDomain(const std::shared_ptr< LuaContext >& ctx) {
    ctx->setFactory(&vFactory);
}
