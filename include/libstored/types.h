#ifndef __LIBSTORED_TYPES_H
#define __LIBSTORED_TYPES_H

#ifdef __cplusplus

#include <libstored/macros.h>
#include <libstored/config.h>
#include <libstored/util.h>

#include <cstdlib>
#include <cstring>
#include <algorithm>

#if __cplusplus >= 201103L
#  include <cinttypes>
#else
#  include <inttypes.h>
#endif

namespace stored {

	struct Type {
		enum type {
			MaskSize = 0x07,
			MaskFlags = 0x78,
			FlagSigned = 0x08,
			FlagInt = 0x10,
			FlagFixed = 0x20,
			FlagFunction = 0x40,

			// int
			Int8 = FlagFixed | FlagInt | FlagSigned | 0,
			Uint8 = FlagFixed | FlagInt | 0,
			Int16 = FlagFixed | FlagInt | FlagSigned | 1,
			Uint16 = FlagFixed | FlagInt | 1,
			Int32 = FlagFixed | FlagInt | FlagSigned | 3,
			Uint32 = FlagFixed | FlagInt | 3,
			Int64 = FlagFixed | FlagInt | FlagSigned | 7,
			Uint64 = FlagFixed | FlagInt | 7,
			Int = FlagFixed | FlagInt | (sizeof(int) - 1),
			Uint = FlagFixed | (sizeof(int) - 1),

			// things with fixed length
			Float = FlagFixed | FlagSigned | 3,
			Double = FlagFixed | FlagSigned | 7,
			Bool = FlagFixed | 0,
			Pointer32 = FlagFixed | 3,
			Pointer64 = FlagFixed | 7,
			Pointer = sizeof(void*) <= 4 ? Pointer32 : Pointer64,

			// (special) things with undefined length
			Void = 0,
			Blob = 1,
			String = 2,
		};

		static bool isFunction(type t) { return t & FlagFunction; }
		static bool isFixed(type t) { return t & FlagFixed; }
		static bool isInt(type t) { return isFixed(t) && (t & FlagInt); }
		static bool isSpecial(type t) { return (t & MaskFlags) == 0; }
		static size_t size(type t) { return !isFixed(t) ? 0 : (t & MaskSize) + 1; }
	};

	namespace impl {
		template <bool signd, size_t size> struct toIntType { static stored::Type::type const type = Type::Void; };
		template <> struct toIntType<true,1> { static stored::Type::type const type = Type::Int8; };
		template <> struct toIntType<false,1> { static stored::Type::type const type = Type::Uint8; };
		template <> struct toIntType<true,2> { static stored::Type::type const type = Type::Int16; };
		template <> struct toIntType<false,2> { static stored::Type::type const type = Type::Uint16; };
		template <> struct toIntType<true,4> { static stored::Type::type const type = Type::Int32; };
		template <> struct toIntType<false,4> { static stored::Type::type const type = Type::Uint32; };
		template <> struct toIntType<true,8> { static stored::Type::type const type = Type::Int64; };
		template <> struct toIntType<false,8> { static stored::Type::type const type = Type::Uint64; };
	}

	template <typename T> struct toType { static Type::type const type = Type::Blob; };
	template <> struct toType<void> { static Type::type const type = Type::Void; };
	template <> struct toType<bool> { static Type::type const type = Type::Bool; };
	template <> struct toType<char> : public impl::toIntType<false,sizeof(char)> {};
	template <> struct toType<signed char> : public impl::toIntType<true,sizeof(char)> {};
	template <> struct toType<unsigned char> : public impl::toIntType<false,sizeof(char)> {};
	template <> struct toType<short> : public impl::toIntType<true,sizeof(short)> {};
	template <> struct toType<unsigned short> : public impl::toIntType<false,sizeof(short)> {};
	template <> struct toType<int> : public impl::toIntType<true,sizeof(int)> {};
	template <> struct toType<unsigned int> : public impl::toIntType<false,sizeof(int)> {};
	template <> struct toType<long> : public impl::toIntType<true,sizeof(long)> {};
	template <> struct toType<unsigned long> : public impl::toIntType<false,sizeof(long)> {};
	template <> struct toType<long long> : public impl::toIntType<true,sizeof(long long)> {};
	template <> struct toType<unsigned long long> : public impl::toIntType<false,sizeof(long long)> {};
	template <> struct toType<float> { static Type::type const type = Type::Float; };
	template <> struct toType<double> { static Type::type const type = Type::Double; };
	template <> struct toType<char*> { static Type::type const type = Type::String; };
	template <typename T> struct toType<T*> { static Type::type const type = Type::Pointer; };

	template <typename T, typename Container, bool Hooks = Config::EnableHooks>
	class Variable {
	public:
		typedef T type;
		Variable(Container& UNUSED_PAR(container), type& buffer)
			: m_buffer(&buffer)
		{
			stored_assert(((uintptr_t)&buffer & (sizeof(type) - 1)) == 0);
		}
		Variable() : m_buffer() {}

		Variable(Variable const& v) { (*this) = v; }
		Variable& operator=(Variable const& v) {
			m_buffer = v.m_buffer;
			return *this;
		}

#if __cplusplus >= 201103L
		Variable(Variable&& v) { (*this) = std::move(v); }
		Variable& operator=(Variable&& v) { this->operator=((Variable const&)v); }
#endif

		type const& get() const {
			stored_assert(valid());
			return buffer();
		}

		template <typename U>
		U as() const { return saturated_cast<U>(get()); }

		operator type const&() const { return get(); }

		void set(type v) {
			stored_assert(valid());
			buffer() = v;
		}

		Variable& operator=(type v) { set(v); return *this; }

		bool valid() const { return m_buffer; }
		Container& container() const { std::abort(); }

	protected:
		type& buffer() const {
			stored_assert(valid());
			return *m_buffer;
		}

	private:
		type* m_buffer;
	};

	template <typename T, typename Container>
	class Variable<T,Container,true> : public Variable<T,Container,false> {
	public:
		typedef Variable<T,Container,false> base;
		typedef typename base::type type;
		
		Variable(Container& container, type& buffer)
			: base(container, buffer)
			, m_container(&container)
		{}
		
		Variable(Variable const& v) { (*this) = v; }
		Variable& operator=(Variable const& v) {
			base::operator=(v);
			m_container = v.m_container;
			return *this;
		}

#if __cplusplus >= 201103L
		Variable(Variable&& v) { (*this) = std::move(v); }
		Variable& operator=(Variable&& v) { this->operator=((Variable const&)v); return *this; }
#endif

		void set(type v) {
			if(Config::HookSetOnChangeOnly)
				if(memcmp(&v, &this->buffer(), sizeof(v)) == 0)
					return;

			base::set(v);
			container().hookSet(toType<T>::type, &this->buffer(), sizeof(type));
		}
		Variable& operator=(type v) { 
			base::operator=(v);
			return *this;
		}

		Container& container() const {
			stored_assert(this->valid());
			return *m_container;
		}
		
		typename Container::Key key() const { return container().bufferToKey(&this->buffer()); }

	private:
		Container* m_container;
	};

	template <typename T, typename Container>
	class Function {
	public:
		typedef T type;

		Function(Container& container, unsigned int f) : m_container(&container), m_f(f) {}
		Function() : m_f() {}

		type get() const {
			stored_assert(valid());
			type value;
			callback(false, value);
			return value;
		}

		size_t get(void* dst, size_t len) const {
			stored_assert(valid());
			return callback(false, dst, len);
		}

		void set(type value) const {
			stored_assert(valid());
			callback(true, value);
		}

		size_t set(void* src, size_t len) {
			stored_assert(valid());
			return callback(true, src, len);
		}

		type operator()() const { return get(); }
		void operator()(type value) const { set(value); }
		
		bool valid() const { return id() >= 0; }

		Container& container() const {
			stored_assert(valid());
			return *m_container;
		}

		size_t callback(bool set, type& value) const {
			stored_assert(valid());
			return container().callback(set, &value, sizeof(type), id());
		}

		size_t callback(bool set, void* buffer, size_t len) {
			stored_assert(valid());
			return container().callback(set, buffer, len, id());
		}

		unsigned int id() const { return m_f; }

	private:
		Container* m_container;
		unsigned int m_f;
	};

	template <typename Container = void>
	class Variant {
	public:
		typedef size_t(Callback)(Container&,bool,uint8_t*,size_t);

		Variant(Container& container, Type::type type, void* buffer, size_t len)
			: m_container(&container), m_buffer(buffer), m_len(len), m_type((uint8_t)type)
		{
			stored_assert(!Type::isFixed(this->type()) || ((uintptr_t)buffer & (Type::size(this->type()) - 1)) == 0);
		}
		
		Variant(Container& container, Type::type type, unsigned int f)
			: m_container(&container), m_f((uintptr_t)f), m_type((uint8_t)type)
		{
			static_assert(sizeof(uintptr_t) >= sizeof(unsigned int), "");
		}

		Variant()
			: m_buffer()
		{
		}

		template <typename T>
		Variant(Variable<T,Container> const& v)
			: m_container(v.valid() ? &v.container() : nullptr)
			, m_buffer(v.valid() ? &v.get() : nullptr)
			, m_len(v.size())
			, m_type(toType<T>::type)
		{}
		
		template <typename T>
		Variant(Function<T,Container> const& f)
			: m_container(f.valid() ? &f.container() : nullptr)
			, m_f(f.valid() ? f.id() : 0)
			, m_type(f.type())
		{}

		size_t get(void* dst, size_t len = 0) const {
			if(Type::isFunction(type())) {
				len = container().callback(false, dst, len, (unsigned int)m_f);
			} else {
				if(Type::isFixed(type())) {
					stored_assert(len == size() || len == 0);
					len = size();
				} else {
					stored_assert(len <= size());
					len = std::min(len, size());
				}
				memcpy(dst, m_buffer, len);
			}
			return len;
		}

		size_t set(void const* src, size_t len = 0) {
			if(isFunction()) {
				len = container().callback(true, const_cast<void*>(src), len, (unsigned int)m_f);
			} else {
				if(Type::isFixed(type())) {
					stored_assert(len == size() || len == 0);
					len = size();
				} else {
					stored_assert(len <= size());
					len = std::min(len, size());
				}

				if(!Config::HookSetOnChangeOnly || memcmp(src, m_buffer, len) != 0) {
					memcpy(m_buffer, src, len);

					if(Config::EnableHooks)
						container().hookSet(type(), m_buffer, len);
				}
			}
			return len;
		}

		Type::type type() const { stored_assert(valid()); return (Type::type)m_type; }
		size_t size() const { stored_assert(valid()); return Type::isFixed(type()) ? Type::size(type()) : m_len; }
		bool valid() const { return m_buffer; }
		bool isFunction() const { stored_assert(valid()); return Type::isFunction(type()); }
		bool isVariable() const { stored_assert(valid()); return !isFunction(); }
		Container& container() const { stored_assert(valid()); return *m_container; }

		template <typename T> Variable<T,Container> variable() const {
			stored_assert(isVariable());
			stored_assert(sizeof(T) == size());
			return Variable<T,Container>(container(), *reinterpret_cast<T*>(m_buffer));
		}

		template <typename T> Function<T,Container> function() const {
			stored_assert(isFunction());
			return Function<T,Container>(container(), (unsigned int)m_f);
		}
		
		typename Container::Key key() const {
			stored_assert(isVariable());
			return container()->bufferToKey(m_buffer);
		}

	private:
		Container* m_container;
		union {
			void* m_buffer;
			uintptr_t m_f;
		};
		size_t m_len;
		uint8_t m_type;
	};
	
	template <>
	class Variant<void> {
	public:
		Variant(Type::type type, void* buffer, size_t len)
			: m_buffer(buffer), m_len(len), m_type((uint8_t)type)
		{
		}
		
		Variant(Type::type type, unsigned int f)
			: m_f((uintptr_t)f), m_type((uint8_t)type)
		{
			static_assert(sizeof(uintptr_t) >= sizeof(unsigned int), "");
		}

		Variant()
			: m_buffer()
		{}

		template <typename Container>
		Variant<Container> apply(Container& container) const {
			static_assert(sizeof(Variant<Container>) == sizeof(Variant<>), "");

			if(!m_buffer)
				return Variant<Container>();
			else if(Type::isFunction((Type::type)m_type))
				return Variant<Container>(container, (Type::type)m_type, (unsigned int)m_f);
			else {
				stored_assert((uintptr_t)m_buffer >= (uintptr_t)&container && (uintptr_t)m_buffer + m_len <= (uintptr_t)&container + sizeof(Container));
				return Variant<Container>(container, (Type::type)m_type, m_buffer, m_len);
			}
		}
		
		size_t get(void* UNUSED_PAR(dst), size_t UNUSED_PAR(len) = 0) const { stored_assert(valid()); return 0; }
		size_t set(void const* UNUSED_PAR(src), size_t UNUSED_PAR(len) = 0) { stored_assert(valid()); return 0; }
		Type::type type() const { stored_assert(valid()); return (Type::type)m_type; }
		size_t size() const { stored_assert(valid()); return Type::isFixed(type()) ? Type::size(type()) : m_len; }
		bool valid() const { return m_buffer; }
		bool isFunction() const { stored_assert(valid()); return Type::isFunction(type()); }
		bool isVariable() const { stored_assert(valid()); return !isFunction(); }
		void* container() const { stored_assert(false); return nullptr; }

	private:
		void* m_dummy; // Make this class the same size as a non-void container.
		union {
			void* m_buffer;
			uintptr_t m_f;
		};
		size_t m_len;
		uint8_t m_type;
	};

} // namespace
#endif // __cplusplus
#endif // __LIBSTORED_TYPES_H
