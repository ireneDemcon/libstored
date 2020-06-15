#include <stored>

#include "SomeStore.h"
#include "AnotherStore.h"

class MyDebugger : public stored::Debugger {
public:
	typedef stored::Debugger base;
	virtual ~MyDebugger() override {}

	void process(char const* frame) {
		processApplication(frame, strlen(frame));
	}

protected:
	virtual void processApplication(void const* frame, size_t len) override {
		printf(">>   %s\n", (char const*)frame);
		base::processApplication(frame, len);
	}

	virtual void respondApplication(void const* frame, size_t len) override {
		printf("<<   %.*s\n", (int)len, (char const*)frame);
	}
};

int main() {
	// Create a few stores.
	stored::SomeStore someStore1;
	stored::SomeStore someStore2;
	stored::AnotherStore anotherStore;

	// Register them to a debugger.
	MyDebugger debugger;
	debugger.map(someStore1, "/SomeStore");
	debugger.map(someStore2, "/OtherInstanceOfSomeStore");
	debugger.map(anotherStore); // Use default name.

	// Some accesses to the stores objects using the full prefix.
	int32_t i;
	// The stored::DebugVariant is a bit more expensive than directly
	// accessing the store's accessors, but allows a template-independant interface,
	// as the debugger will operate only on such an interface.
	stored::DebugVariant i1 = debugger.find("/SomeStore/i");
	i1.get(&i, sizeof(i));
	printf("/SomeStore/i = %" PRId32 "\n", i);
	i++;
	i1.set(&i, sizeof(i));
	printf("/SomeStore/i = %" PRId32 "\n", someStore1.i().get());
	
	stored::DebugVariant i2 = debugger.find("/OtherInstanceOfSomeStore/i");
	i2.get(&i, sizeof(i));
	printf("/OtherInstanceOfSomeStore/i = %" PRId32 "\n", i);

	stored::DebugVariant j = debugger.find("/AnotherStore/j");
	j.get(&i, sizeof(i));
	printf("/AnotherStore/j = %" PRId32 "\n", i);

	// DebugVariants are small, copyable and assignable, so they can be used in
	// std::map, for example.
	i2 = i1; // let i2 point to /SomeStore/i
	i2.get(&i, sizeof(i));
	printf("i2 = %" PRId32 "\n", i);

	// Now process some Embedded Debugger messages
	debugger.process("?");
	debugger.process("r/AnotherStore/j");
	debugger.process("wf00f/SomeStore/i");
	debugger.process("r/SomeStore/i");

	return 0;
}

