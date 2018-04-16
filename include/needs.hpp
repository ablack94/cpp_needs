// Andrew Black
// April 15, 2018

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <map>

/** @brief Untemplated base class for all value wrappers.
 *  @details
 *  This is the abstract base class for all value wrappers that
 *  does not use templates. This allows us to use value wrappers
 *  in a collection, assuming we know the type of the pointer being stored.
 */
struct base_wrapper {
	virtual ~base_wrapper() = default;
	virtual void* getRaw() = 0;
};

/** @brief Base value wrapper interface.
 *  @ptaram T type of the value wrapper pointer.
 *  @details
 *  This is the base value wrapper interface that all wrappers
 *  must inherit from. It simply contains a virtual destructor
 *  so that child classes can be instantiated and cleaned up from
 *  the base pointer.
 *
 *  The template parameter defines the pointer returned by the get function.
 */
template <typename T>
struct tbase_wrapper : public base_wrapper {
	//virtual ~tbase_wrapper() = default;
	void* getRaw() override { return (void*)get(); }
	virtual T* get() = 0;
};

/** @brief raw value wrapper class.
 *  @tparam T type of the pointer to return.
 *  @tparam X type of the value to store. Defaults to \p T
 *  @details
 *  Wraps a raw value. The param \p T defines the base class of the pointer.
 *  The second param \p X defaults to type \p T. If \p T is an abstract class
 *  then the value of \p X must be of the concrete type to store.
 *
 *  @remark
 *  This class takes ownership of the value. The value is either copy constructed in
 *  or moved in from the caller. Either way, the value is cleaned up when this object
 *  instance is destroyed.
 */
template <typename T, typename X=T>
struct value_wrapper : tbase_wrapper<T> {
	value_wrapper(X v) {
		value = std::move(v);
	}

	T* get() { return &value; }
		
protected:
	X value;
};

/** @brief 
 *  @param T type of the pointer to return.
 *  @details
 *  Wraps a raw pointer or reference value.
 * 
 *  @remark
 *  This class does not take ownership of the value. It is the caller's
 *  responsibility to manage the lifetime of the wrapped value.
 */
template <typename T>
struct ptr_wrapper : tbase_wrapper<T> {
	ptr_wrapper(T& v) { ptr = &v; }

	ptr_wrapper(T* v) { 
		if(v != nullptr) {
			ptr = v;
		} else {
			throw std::invalid_argument("Null raw pointer!");
		}
	}

	T* get() override { return ptr; }
protected:
	T* ptr;
};

template <typename T>
struct uptr_wrapper : tbase_wrapper<T> {
	uptr_wrapper(std::unique_ptr<T> v) {
		if(v) {
			uptr = std::move(v);
		} else {
			throw std::invalid_argument("Null unique pointer!");
		}
	}
	
	T* get() override { return uptr.get(); }
protected:
	std::unique_ptr<T> uptr;
};

template <typename T>
struct sptr_wrapper : tbase_wrapper<T> {
	sptr_wrapper(std::shared_ptr<T> v) {
		if(v) {
			sptr = std::move(v);
		} else {
			throw std::invalid_argument("Null shared pointer!");
		}
	}
	
	T* get() override { return sptr.get(); }
protected:
	std::shared_ptr<T> sptr;
};

/** @brief Helper template that defines an empty void function.
 *  @details
 *  This is used to create an empty function for every type that we instantiate
 *  using this helper template. We use the address of the static function as
 *  a UID for the given type.
 */
template <typename T>
struct type { static void uid() { } };

/** @brief Helper function that gets the UID for a given type.
 *  @details
 *  This gets the address of the static uid function defined by the
 *  helper struct 'type'. The UID is returned as a size_t value.
 */
template <typename T>
size_t type_id() { return (size_t)&type<T>::uid; }

/** @brief Template class representing classes to be injected in to an object.
 */
template <typename ...Needs>
struct needs {
	needs() {
		// Add all types to our map and initialize to null pointers.
		int i = 0;
		for(auto t : { type_id<Needs>()... }) {
			values[t] = std::unique_ptr<base_wrapper>(nullptr);
		}
	}

	/** @brief Gets the value of the dependency of type \p X.
	 *  @returns the pointer for the dependency given by type \p X.
	 *  @throws std::runtime_error if type \p X is not part of class definition.
	 *  @throws std::runtime_error if type \p X was never set to a value.
	 */
	template <typename X>
	X* get() {
		size_t key = verify_type<X>();
		auto ptr = values[key].get();
		if(ptr != nullptr) {
			return static_cast<X*>(ptr->getRaw());
		} else {
			throw std::runtime_error("Need type never set!");
		}
	}

	/** @brief Sets the dependency given by type \p T to the value \p v.
	 *  @tparam  T  Type of the dependency to set.
	 *  @tparam  X  Type of the concrete class to set to the dependency.
	 *  @details
	 *  Sets the dependency given by type \p T to the value \p v of type \p X.
	 *  By default \p X = \p T. However, in cases where polymorphism is used,
 	 *  there are situations where we want to set a dependency of type \p T to
	 *  a value given by a concrete subclass given by \p X.
	 */
	template <typename T, typename X=T>
	void set(X v) {
		size_t key = verify_type<T>();
		values[key].reset(new value_wrapper<T, X>(std::move(v)));
	}

	/** @brief Sets the dependency given by type \p T to the value \p v.
	 *  @tparam  T  Type of the dependency to set.
	 *  @tparam  X  Type of the concrete class to set to the dependency.
	 *  @details
	 *  Sets the dependency given by type \p T to the value \p v of type \p X.
	 *  For pointers this template would normally not be required, but this is here
	 *  to avoid ambiguous calls with the set<T, X>(X v). This explicitly requires
	 *  the value to be a pointer of type X which resolves the ambiguity.
	 */
	template <typename T, typename X=T>
	void set(X* v) {
		size_t key = verify_type<T>();
		values[key].reset(new ptr_wrapper<T>(v));
	}

	template <typename T>
	void set(T& v) {
		size_t key = verify_type<T>();
		values[key].reset(new ptr_wrapper<T>(v));
	}

	template <typename T>
	void set(T* v) {
		size_t key = verify_type<T>();
		values[key].reset(new ptr_wrapper<T>(std::move(v)));
	}

	template <typename T>
	void set(std::unique_ptr<T> v) {
		size_t key = verify_type<T>();
		values[key].reset(new sptr_wrapper<T>(std::move(v)));
	}
	
	
protected:
	std::map<size_t, std::unique_ptr<base_wrapper>> values;

	/** @brief Helper function that returns the key for the given type or errors.
	 *  @details
	 *  This is a helper function to get the key in our values map from a type.
	 */
	template <typename T>
	size_t verify_type() {
		size_t key = type_id<T>();
		if(values.count(key) != 0) {
			return key;
		} else {
			throw std::invalid_argument("Type specified is not a need!");
		}
		return key;
	}
};


