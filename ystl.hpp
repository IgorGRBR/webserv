#ifndef YSTL_HPP
#define YSTL_HPP

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>

// This is my own, tiny, "standard template"-esque library. It contains various useful utilities that are not present
// in C++ 98, which is what we are forced to use for this project.

// Internal use only. Do not use this, ever. It **will** cause pain and suffering.
template <typename T>
inline static T undefinedBehaviorMachine() {
	// This function exists because C++ sucks balls. In -Wall mode there is no effective way to create
	// a concept of a variant type in C++. You can't use unions, because for some reason they require
	// you to use only POD types. You can't use default-init fields, because they require your types to
	// have a default constructor, which often doesn't make sense for a variant-like type.

	// What this function does is it returns a deliberately-uninitialized memory, that can be used in
	// places where you can verify it would never be used (such as the following Option and Result types).

	// Do not use this function outside of `ystl`. Ever.
	static char iHateCPP[sizeof(T)];
	return *reinterpret_cast<T*>(static_cast<void*>(&iHateCPP));
}

// A simple struct and its singular, global value. Its primary use is replacing empty `Option` constructors with
// `NONE`, so that it iss a bit more clear at the construction site when the optional value is missing.
// Basically, you don't have to write `Option<T> maybeValue = Option<T>();`, you can
// just replace it with `Option<T> maybeValue = NONE;`
struct NoneOpt {};
static const NoneOpt NONE;

// `Option<T>` represents a value of type `T`, that may, or may not be missing. The benefits of using this over
// nullable pointers are:
//	- You don't need to worry about managing memory as much - the inner value is located on the stack;
//	- It provides a better code clarity - unlike pointers, seeing Option used in the code clearly indicates that its value
//	  may, or may not be present.
template <typename T>
struct Option {
	// Constructor with no parameters constructs an `Option` object with missing value
	Option(): present(false), value(undefinedBehaviorMachine<T>()) {}

	// A helper constructor for the use of `NONE`. It works this way due to C++ implicitly casting types, if the
	// target type of the cast has a constructor, that has a single parameter of the source type.
	Option(NoneOpt): present(false), value(undefinedBehaviorMachine<T>()) {}

	// Constructor with single parameter constructs an `Option` object with the passed value
	Option(T v): present(true), value(v) {}

	// Safely returns the inner value, crashing the program if it is missing.
	T& get() {
		if (!present) {
			std::cerr << "Attempt to unwrap non-existent optional" << std::endl;
			std::exit(1);
		}
		return value;
	}

	// Safely returns the inner value, crashing the program if it is missing.
	const T& get() const {
		if (!present) {
			std::cerr << "Attempt to unwrap non-existent optional" << std::endl;
			std::exit(1);
		}
		return value;
	}

	// Returns a pointer to the inner value, that may be NULL if the value is missing.
	T* tryGet() {
		return present ? &value : NULL;
	}

	// Returns true if the value is present.
	bool isSome() const {
		return present;
	}

	// Returns true if the value is missing.
	bool isNone() const {
		return !present;
	}

	// Returns the reference to inner value if it is present, or returns the `defaultValue` otherwise.
	T& getOr(T& defaultValue) {
		return present ? value : defaultValue;
	}

	// Returns the reference to inner value if it is present, or returns the `defaultValue` otherwise.
	const T& getOr(const T& defaultValue) const {
		return present ? value : defaultValue;
	}
private:
	bool present;
	T value;
};

// The `Result<T, E>` represents a value that is either a successful result of some computation, or the error
// that might have occured during its computation.
template <typename T, typename E>
class Result {
public:

	// This constructs the `Result` object as successful with the provided value.
	Result(T v): val(v), err(undefinedBehaviorMachine<E>()) {
		resultIsOk = true;
	};

	// This constructs the `Result` object as failed with the provided error.
	Result(E e): val(undefinedBehaviorMachine<T>()), err(e) {
		resultIsOk = false;
	};

	// Returns true if the result is successful.
	bool isOk() {
		return resultIsOk;
	}

	// Returns true if the result is unsuccessful.
	bool isError() {
		return !resultIsOk;
	}

	// Safely returns the successful value, crashing the program if it is missing.
	T& getValue() {
		if (!resultIsOk) {
			std::cerr << "Attempt to retrieve a value when error is present." << std::endl;
			std::exit(1);
		}
		return val;
	}

	// Safely returns the successful value, crashing the program if it is missing.
	const T& getValue() const {
		if (!resultIsOk) {
			std::cerr << "Attempt to retrieve a value when error is present." << std::endl;
			std::exit(1);
		}
		return val;
	}

	// Returns a pointer to inner value if the result is successful, or NULL otherwise.
	T* tryGetValue() {
		return resultIsOk ? &val : NULL;
	}

	// Safely returns the error, crashing the program if it is missing.
	E& getError() {
		if (resultIsOk) {
			std::cerr << "Attempt to retrieve an error when value is present." << std::endl;
			std::exit(1);
		}
		return err;
	}

	// Safely returns the error, crashing the program if it is missing.
	const E& getError() const {
		if (resultIsOk) {
			std::cerr << "Attempt to retrieve an error when value is present." << std::endl;
			std::exit(1);
		}
		return err;
	}

	// Returns a pointer to inner error if the result was unsuccessful, or NULL otherwise.
	E* tryGetError() {
		return !resultIsOk ? &err : NULL;
	}

private:
	bool resultIsOk;
	T val;
	E err;
};

// `UniquePtr<T>` is a smart pointer that binds the lifetime of its unique resource to its scope. Unique pointers
// can transfer their ownership to other unique pointers by copying or assignment. Access to the inner pointer should
// only happen through this class (meaning that taking out the inner pointer and stroring it elsewhere is a terrible idea,
// one should explicitly move out the value first).
template <typename T>
class UniquePtr {
public:
	// Constructs a `UniquePtr` that owns nothing.
	UniquePtr(): ptr(NULL) {};

	// Constructs a `UniquePtr` that owns `p`. After invoking this constructor, it is not recommended to store or use `p` anywhere
	// outside of the `UniquePtr`.
	UniquePtr(T* p): ptr(p) {};

	// Copy constructor acts as a "moving" constructor - by creating a copy of the unique pointer, the ownership of that pointer
	// is being transferred from original object to its copy.
	UniquePtr(const UniquePtr& other): ptr(NULL) {
		UniquePtr<T>& otherMut = const_cast<UniquePtr<T>& >(other); // `const_cast`, ewwww....
		ptr = otherMut.move();
	};

	// Assignment operator transfers the ownership from the assignee to the assigned object.
	UniquePtr& operator=(const UniquePtr& other) {
		if ( ptr) {
			delete ptr;
		}
		UniquePtr<T>& otherMut = const_cast<UniquePtr<T>& >(other); // `const_cast`, ewwww....
		ptr = otherMut.move();
		return *this;
	}

	// Deletes the inner object if unique pointer onws it.
	~UniquePtr() {
		if (ptr) {
			delete ptr;
		}
	};

	// Arrow operator allows `UniquePtr` to act as a normal pointer, letting you ergonomically access the inner
	// fields and methods.
	T* operator->() {
		return ptr;
	}

	// Arrow operator allows `UniquePtr` to act as a normal pointer, letting you ergonomically access the inner
	// fields and methods.
	const T* operator->() const {
		return ptr;
	}

	// Performs an explicit move operation - unique pointer drops the ownership of the inner pointer and returns it
	// to the caller.
	inline T* move() {
		T* result = ptr;
		ptr = NULL;
		return result;
	}

	T& ref() {
		return *ptr;
	}

	const T& ref() const {
		return *ptr;
	}

	// Returns true if unique pointer owns the inner object.
	bool isMoved() const {
		return ptr == NULL;
	}
private:
	T* ptr;
};

// `SharedPtr<T>` is a smart pointer that tracks the number of references to the inner pointer, only releasing it when
// this count reaches zero. Essentially this allows for a reference-counting-based garbage collection. Keep in mind, there
// is no concept of WeakPtr in ystl, and this smart pointer does not perform cycle detection, making cycle leaks possible, thus
// one should use this pointer with care.
template <typename T>
class SharedPtr {
public:

	// Constructs a `SharedPtr` with pointer `p`, and initializes its reference count to 1.
	SharedPtr(T* p): ptr(p), refCount(new uint(1)) {};

	// Constructs a copy of another `SharedPtr`, and increments their common reference count.
	SharedPtr(const SharedPtr& other): ptr(other.ptr), refCount(other.refCount) {
		(*refCount)++;
	};

	// Constructs a copy of another `SharedPtr`, and increments their common reference count.
	SharedPtr& operator=(const SharedPtr& other) {
		tryDelete();
		ptr = other.ptr;
		refCount = other.refCount;
		(*refCount)++;
		return *this;
	}

	// Attempts to delete the inner value.
	~SharedPtr() {
		tryDelete();
	}

	// Arrow operator allows `SharedPtr` to act as a normal pointer, letting you ergonomically access the inner
	// fields and methods.
	T* operator->() {
		return ptr;
	}

	// Arrow operator allows `SharedPtr` to act as a normal pointer, letting you ergonomically access the inner
	// fields and methods.
	const T* operator->() const {
		return ptr;
	}

	// Retrieves the reference count of the pointer.
	uint getRefCount() const {
		return *refCount;
	}
private:
	T* ptr;
	uint* refCount;

	// Decrements the reference count of a pointer, and deletes it if reference count reaches zero.
	void tryDelete() {
		if (--(*refCount) == 0) {
			delete ptr;
			delete refCount;
			ptr = NULL;
		}
	}
};

// Convers the input string to lowercase.
std::string strToLower(const std::string&);

// Trims string from the specified character.
std::string trimString(const std::string&, char);

enum FSType {
	FS_NONE,
	FS_FILE,
	FS_DIRECTORY,
};

// Check if a specified path string leads to a directory, file or nothing.
FSType checkFSType(const std::string&);

#endif
