#ifndef YSTL_HPP
#define YSTL_HPP

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>

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

struct NoneOpt {};
static const NoneOpt NONE;

template <typename T>
struct Option {
	Option(): present(false), value(undefinedBehaviorMachine<T>()) {}
	Option(NoneOpt): present(false), value(undefinedBehaviorMachine<T>()) {}
	Option(T v): present(true), value(v) {}

	T& get() {
		if (!present) {
			std::cerr << "Attempt to unwrap non-existent optional" << std::endl;
			std::exit(1);
		}
		return value;
	}

	const T& get() const {
		if (!present) {
			std::cerr << "Attempt to unwrap non-existent optional" << std::endl;
			std::exit(1);
		}
		return value;
	}

	T* tryGet() {
		return present ? &value : NULL;
	}

	bool isSome() const {
		return present;
	}

	bool isNone() const {
		return !present;
	}

	T& getOr(T& defaultValue) {
		return present ? value : defaultValue;
	}

	const T& getOr(const T& defaultValue) const {
		return present ? value : defaultValue;
	}
private:
	bool present;
	T value;
};

template <typename T, typename E>
class Result {
public:
	Result(T v): val(v), err(undefinedBehaviorMachine<E>()) {
		resultIsOk = true;
	};

	// Result(E e): val(*reinterpret_cast<T*>(iHateCPP())) {
	Result(E e): val(undefinedBehaviorMachine<T>()), err(e) {
		// err = e;
		resultIsOk = false;
	};

	bool isOk() {
		return resultIsOk;
	}

	bool isError() {
		return !resultIsOk;
	}

	T& getValue() {
		if (!resultIsOk) {
			std::cerr << "Attempt to retrieve a value when error is present." << std::endl;
			std::exit(1);
		}
		return val;
	}

	const T& getValue() const {
		if (!resultIsOk) {
			std::cerr << "Attempt to retrieve a value when error is present." << std::endl;
			std::exit(1);
		}
		return val;
	}

	T* tryGetValue() {
		return resultIsOk ? &val : NULL;
	}

	E& getError() {
		if (resultIsOk) {
			std::cerr << "Attempt to retrieve an error when value is present." << std::endl;
			std::exit(1);
		}
		return err;
	}

	const E& getError() const {
		if (resultIsOk) {
			std::cerr << "Attempt to retrieve an error when value is present." << std::endl;
			std::exit(1);
		}
		return err;
	}

	E* tryGetError() {
		return !resultIsOk ? &err : NULL;
	}

private:
	bool resultIsOk;
	T val;
	E err;
};

template <typename T>
class UniquePtr {
public:
	UniquePtr(): ptr(NULL) {};

	UniquePtr(T* p): ptr(p) {};

	UniquePtr(const UniquePtr& other): ptr(NULL) {
		UniquePtr<T>& otherMut = const_cast<UniquePtr<T>& >(other); // This is awful
		ptr = otherMut.move();
	};

	UniquePtr& operator=(const UniquePtr& other) {
		if (ptr) {
			delete ptr;
		}
		UniquePtr<T>& otherMut = const_cast<UniquePtr<T>& >(other); // This is awful
		ptr = otherMut.move();
		return *this;
	}

	~UniquePtr() {
		if (ptr) {
			delete ptr;
		}
	};

	T* operator->() {
		return ptr;
	}

	const T* operator->() const {
		return ptr;
	}

	inline T* move() {
		T* result = ptr;
		ptr = NULL;
		return result;
	}

	bool isMoved() const {
		return ptr == NULL;
	}
private:
	T* ptr;
};

template <typename T>
class SharedPtr {
public:
	SharedPtr(T* p): ptr(p), refCount(new uint(1)) {};

	SharedPtr(const SharedPtr& other): ptr(other.ptr), refCount(other.refCount) {
		(*refCount)++;
	};

	SharedPtr& operator=(const SharedPtr& other) {
		tryDelete();
		ptr = other.ptr;
		refCount = other.refCount;
		(*refCount)++;
		return *this;
	}

	~SharedPtr() {
		tryDelete();
	}

	T* operator->() {
		return ptr;
	}

	const T* operator->() const {
		return ptr;
	}

	uint getRefCount() const {
		return *refCount;
	}
private:
	T* ptr;
	uint* refCount;

	void tryDelete() {
		if (--(*refCount) == 0) {
			delete ptr;
			delete refCount;
			ptr = NULL;
		}
	}
};

std::string strToLower(const std::string&);

#endif