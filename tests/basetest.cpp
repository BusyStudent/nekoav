#include <gtest/gtest.h>
#include "../nekoav/detail/base.hpp"
#include "../nekoav/elements/appsrc.hpp"
#include "../nekoav/resource.hpp"
#include "../nekoav/factory.hpp"

using namespace NEKO_NAMESPACE;

TEST(Base_ABIV1, ABIV1) {
    class TestClass final : public _abiv1::ThreadingImpl<Element> {
    public:
        TestClass() {
            setName("TestClass");
            auto in = addInput("sink");
        }
        Error onSinkPushed(Pad *, View<Resource> resourceView) override {
            printf("Data arrived");
            return Error::Ok;
        }
    }; 
    class Data final : public Resource {
    public:

    };

    auto appsrc = GetElementFactory()->createElement<AppSource>();
    TestClass cls;
    cls.setState(State::Running);
    appsrc->setState(State::Running);
    
    auto ok = LinkElements(appsrc, &cls);
    ASSERT_EQ(ok, Error::Ok);

    // Push data here
    auto data = std::make_shared<Data>();
    appsrc->push(data);
    

    cls.setState(State::Null);
    appsrc->setState(State::Null);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}