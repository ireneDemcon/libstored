#ifndef LIBSTORED_COMPONENTS_H
#define LIBSTORED_COMPONENTS_H
/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2021  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef __cplusplus

#include <libstored/macros.h>

#if STORED_cplusplus >= 201402L

#if defined(_MSC_VER) && _MSC_VER < 1910
#  error Unsupport Visual Studio version. Upgrade to 2017, or do not compile for C++14.
// Because of this: https://devblogs.microsoft.com/cppblog/expression-sfinae-improvements-in-vs-2015-update-3/
//
// However, even with VS 2017, using constexpr calls in an std::enable_if_t
// template parameter still does not work similar to gcc. As a workaround,
// these enable_if_ts are wrapped in a decltype and used as return type or
// dummy parameter. Not every readable, but it works at least.
#endif

#include <libstored/config.h>
#include <libstored/types.h>
#include <libstored/util.h>

#include <cmath>
#include <type_traits>
#include <utility>

namespace stored {

	namespace impl {
		template <char Id, size_t index0, char... Ids>
		struct find_index {};

		template <char Id, size_t index0, char Id0, char... Ids>
		struct find_index<Id,index0,Id0,Ids...> {
			enum { value = find_index<Id, index0 + 1, Ids...>::value };
		};

		template <char Id, size_t index0, char... Ids>
		struct find_index<Id,index0,Id,Ids...> {
			enum { value = index0 };
		};

		template <char Id, char... Ids>
		struct has_id { enum { value = 0 }; };

		template <char Id, char... Ids>
		struct has_id<Id,Id,Ids...> { enum { value = 1 }; };

		template <char Id, char Id0, char... Ids>
		struct has_id<Id,Id0,Ids...> { enum { value = has_id<Id,Ids...>::value }; };

		template <char... Id>
		struct ids { enum { size = sizeof...(Id) }; };

		template <typename A, typename B>
		struct merge_ids {};

		template <char... A, char... B>
		struct merge_ids<ids<A...>, ids<B...>> {
			using type = ids<A..., B...>;
		};

		template <char... Id>
		struct is_unique { enum { value = 1 }; };

		template <char Id0, char... Id>
		struct is_unique<Id0, Id...> { enum { value = !has_id<Id0, Id...>::value && is_unique<Id...>::value }; };

		template <typename T>
		struct is_unique_ids {};

		template <char... Id>
		struct is_unique_ids<ids<Id...>> { enum { value = is_unique<Id...>::value }; };

		template <typename Subset, typename Set>
		struct is_subset {};

		template <char... Set>
		struct is_subset<ids<>, ids<Set...>> { enum { value = 1 }; };

		template <char S0, char... Subset, char... Set>
		struct is_subset<ids<S0, Subset...>, ids<Set...>> {
			enum { value = has_id<S0, Set...>::value && is_subset<ids<Subset...>, ids<Set...>>::value };
		};

		template <typename Select, typename All>
		struct optional_subset {};

		template <char S0, char... Select, char... All>
		struct optional_subset<ids<S0, Select...>, ids<All...>> {
			static_assert(is_subset<ids<S0, Select...>, ids<All...>>::value, "");
			using type = ids<S0, Select...>;
		};

		template <char... All>
		struct optional_subset<ids<>, ids<All...>> {
			using type = ids<All...>;
		};

	} // namespace

	using impl::ids;

	template <typename FreeObjects_, typename FreeObjects_::Flags flags_, bool PostponedBind = true>
	class BoundObjects;

	template <typename ObjectType, char... Id>
	class FreeObjects {
	public:
		using This = FreeObjects;
		using FreeObject = ObjectType;
		using type = typename ObjectType::type;
		using Container = typename ObjectType::Container;
		using Ids = ids<Id...>;
		using Flags = unsigned long long;

		template <Flags flags>
		using Bound = BoundObjects<FreeObjects, flags, sizeof(FreeObject) < sizeof(typename FreeObject::Bound_type)>;

		static_assert(impl::is_unique<Id...>::value, "");

	private:
		FreeObject m_objects[sizeof...(Id) == 0 ? 1 : sizeof...(Id)] {};

		template <char Id_, typename... Args, std::enable_if_t<!impl::has_id<Id_, Id...>::value, int> = 0>
		constexpr size_t init(Args&&...) noexcept {
			return 0;
		}

		template <char Id_, size_t PN, size_t NN, std::enable_if_t<impl::has_id<Id_, Id...>::value, int> = 0>
		constexpr size_t init(char const (&prefix)[PN], char const (&name)[NN]) noexcept {
			char buf[PN + NN] = {};
			size_t len = 0;

			for(; len < PN && prefix[len]; len++)
				buf[len] = prefix[len];

			for(size_t i = 0; i < NN && name[i]; len++, i++)
				buf[len] = name[i];

			auto o = find(buf, FreeObject());

			if(Config::EnableAssert)
				for(size_t i = 0; i < sizeof...(Id); i++) {
					// Check if name resolution is unique.
					stored_assert(!o.valid() || m_objects[i] != o);
				}

			constexpr size_t ix = index<Id_>();
			m_objects[ix] = o;
			return ix;
		}

		template <size_t N>
		static constexpr auto find(char const (&name)[N], FreeVariable<type,Container>) noexcept {
			return Container::template freeVariable<type>(name, N);
		}

		template <size_t N>
		static constexpr auto find(char const (&name)[N], FreeFunction<type,Container>) noexcept {
			return Container::template freeFunction<type>(name, N);
		}

	public:
		template <size_t N, char... OnlyId, char... IdMap, typename... LongNames,
			std::enable_if_t<sizeof...(IdMap) == sizeof...(LongNames), int> = 0>
		static constexpr FreeObjects create(char const (&prefix)[N], ids<OnlyId...>, ids<IdMap...>, LongNames&&... longNames) noexcept {
			static_assert(impl::is_unique<OnlyId...>::value, "");
			static_assert(impl::is_unique<IdMap...>::value, "");
			FreeObjects fo;
			size_t dummy[] = {0, (impl::has_id<IdMap,OnlyId...>::value ? fo.init<IdMap>(prefix, std::forward<LongNames>(longNames)) : 0)...};
			(void)dummy;
			return fo;
		}

		template <char... OnlyId, size_t N>
		static constexpr FreeObjects create(char const (&prefix)[N]) noexcept {
			using Name = char[2];
			return create(prefix, typename impl::optional_subset<ids<OnlyId...>, ids<Id...>>::type(), ids<Id...>(), Name{Id}...);
		}

		template <char... OnlyId, size_t N, typename... LongNames,
			std::enable_if_t<(sizeof...(LongNames) > 0 && sizeof...(LongNames) == sizeof...(Id)), int> = 0>
		static constexpr FreeObjects create(char const (&prefix)[N], LongNames&&... longNames) noexcept {
			return create(prefix, typename impl::optional_subset<ids<OnlyId...>, ids<Id...>>::type(), ids<Id...>(), std::forward<LongNames>(longNames)...);
		}

		static constexpr size_t size() noexcept {
			return sizeof...(Id);
		}

		template <char Id_>
		static constexpr bool has() noexcept {
			return (bool)impl::has_id<Id_, Id...>::value;
		}

		template <char Id_>
		static constexpr size_t index() noexcept {
			return impl::find_index<Id_, 0, Id...>::value;
		}

		constexpr Flags flags() const noexcept {
			Flags f = 0;
			static_assert(sizeof(f) * 8U >= sizeof...(Id), "");

			for(size_t i = 0; i < sizeof...(Id); i++)
				if(m_objects[i].valid())
					f |= (decltype(f))1 << i;

			return f;
		}

		template <char Id_>
		constexpr bool valid() const noexcept {
			return m_objects[index<Id_>()].valid();
		}

		template <char Id_>
		static constexpr decltype(std::enable_if_t<has<Id_>(), bool>()) valid(Flags flags) noexcept {
			return flags & (1ULL << index<Id_>());
		}

		template <char Id_>
		static constexpr decltype(std::enable_if_t<!has<Id_>(), bool>()) valid(Flags) noexcept {
			return false;
		}

		constexpr size_t validSize() const noexcept {
			size_t cnt = 0;

			for(size_t i = 0; i < size(); i++)
				if(m_objects[i].valid())
					cnt++;

			return cnt;
		}

		static constexpr size_t validSize(Flags flags) noexcept {
			size_t count = 0;

			for(size_t i = 0; i < size(); i++)
				if((flags & (1ULL << i)))
					count++;

			return count;
		}

		template <char Id_>
		constexpr size_t validIndex() const noexcept {
			size_t vi = 0;

			for(size_t i = 0; i < index<Id_>(); i++)
				if(m_objects[i].valid())
					vi++;

			return vi;
		}

		template <char Id_>
		static constexpr decltype(std::enable_if_t<has<Id_>(), size_t>()) validIndex(Flags flags) noexcept {
			size_t count = 0;

			for(size_t i = 0; i < index<Id_>(); i++)
				if((flags & (1ULL << i)))
					count++;

			return count;
		}

		template <char Id_>
		static constexpr decltype(std::enable_if_t<!has<Id_>(), size_t>()) validIndex(Flags) noexcept {
			return 0;
		}

		template <char Id_>
		constexpr FreeObject object() const noexcept {
			return m_objects[index<Id_>()];
		}

		template <char Id_>
		auto object(Container& container) const noexcept {
			return object<Id_>().apply(container);
		}
	};

	template <typename T, typename Container, char... Id>
	using FreeVariables = FreeObjects<FreeVariable<T, Container>, Id...>;

	template <typename T, typename Container, char... Id>
	using FreeFunctions = FreeObjects<FreeFunction<T, Container>, Id...>;

	template <typename... B>
	class BoundObjectsList;

	template <typename... F>
	class FreeObjectsList {};

	template <typename F0>
	class FreeObjectsList<F0> : public F0 {
	public:
		using Head = F0;
		using Tail = void;

		constexpr FreeObjectsList() noexcept = default;

		constexpr FreeObjectsList(Head&& head) noexcept
			: Head(std::move(head))
		{}

		constexpr FreeObjectsList(Head const& head) noexcept
			: Head(head)
		{}
	};

	template <typename F0, typename... F>
	class FreeObjectsList<F0, F...> {
	public:
		using Head = F0;
		using Tail = FreeObjectsList<F...>;
		using Ids = typename impl::merge_ids<typename Head::Ids, typename Tail::Ids>::type;
		using Flags = typename Head::Flags;
		using Container = typename Head::Container;

		template <Flags flags>
		using Bound = BoundObjectsList<typename Head::template Bound<flags>, typename Tail::template Bound<(flags >> Head::Ids::size)>>;

		static_assert(impl::is_unique_ids<Ids>::value, "");

	private:
		Head m_head;
		Tail m_tail;

		constexpr FreeObjectsList(Head&& head, Tail&& tail) noexcept
			: m_head{std::move(head)}, m_tail{std::move(tail)}
		{}

	public:
		constexpr FreeObjectsList() noexcept = default;

		static constexpr size_t size() noexcept {
			return Head::size() + Tail::size();
		}

		template <size_t N, char... OnlyId, char... IdMap, typename... LongNames,
			std::enable_if_t<sizeof...(IdMap) == sizeof...(LongNames), int> = 0>
		static constexpr FreeObjectsList create(char const (&prefix)[N], ids<OnlyId...>, ids<IdMap...>, LongNames&&... longNames) noexcept {
			static_assert(impl::is_unique<OnlyId...>::value, "");
			static_assert(impl::is_unique<IdMap...>::value, "");
			return FreeObjectsList{
				Head::template create(prefix, ids<OnlyId...>(), ids<IdMap...>(), longNames...),
				Tail::template create(prefix, ids<OnlyId...>(), ids<IdMap...>(), longNames...)
			};
		}

		template <char... OnlyId, size_t N>
		static constexpr FreeObjectsList create(char const (&prefix)[N]) noexcept {
			static_assert(impl::is_unique<OnlyId...>::value, "");
			return FreeObjectsList{
				Head::template create<OnlyId...>(prefix),
				Tail::template create<OnlyId...>(prefix)
			};
		}

		template <char... OnlyId, size_t N, typename... LongNames,
			std::enable_if_t<(sizeof...(LongNames) > 0 && sizeof...(LongNames) == Ids::size), int> = 0>
		static constexpr FreeObjectsList create(char const (&prefix)[N], LongNames&&... longNames) noexcept {
			static_assert(impl::is_unique<OnlyId...>::value, "");
			return FreeObjectsList{
				Head::create(prefix, typename impl::optional_subset<ids<OnlyId...>, Ids>::type(), Ids(), longNames...),
				Tail::create(prefix, typename impl::optional_subset<ids<OnlyId...>, Ids>::type(), Ids(), longNames...)
			};
		}

		template <char Id>
		static constexpr bool has() noexcept {
			return Head::template has<Id>() || Tail::template has<Id>();
		}

		template <char Id>
		static constexpr decltype(std::enable_if_t<Head::template has<Id>(), size_t>()) index() noexcept {
			return Head::template index<Id>();
		}

		template <char Id>
		static constexpr decltype(std::enable_if_t<!Head::template has<Id>(), size_t>()) index() noexcept {
			return Tail::template index<Id>();
		}

		constexpr Flags flags() const noexcept {
			static_assert(size() <= sizeof(Flags) * 8, "");
			return m_head.flags() | (m_tail.flags() << m_head.size());
		}

		template <char Id>
		static constexpr decltype(std::enable_if_t<Head::template has<Id>(), bool>()) valid(Flags flags) noexcept {
			return Head::template valid<Id>(flags);
		}

		template <char Id>
		static constexpr decltype(std::enable_if_t<!Head::template has<Id>(), bool>()) valid(Flags flags) noexcept {
			return Tail::template valid<Id>(flags >> Head::size());
		}

		constexpr size_t validSize() noexcept {
			return m_head.validSize() + m_tail.validSize();
		}

		static constexpr size_t validSize(Flags flags) noexcept {
			return Head::validSize(flags) + Tail::validSize(flags >> Head::size());
		}

		template <char Id>
		constexpr decltype(std::enable_if_t<Head::template has<Id>(), size_t>()) validIndex() const noexcept {
			return m_head.template validIndex<Id>();
		}

		template <char Id>
		constexpr decltype(std::enable_if_t<!Head::template has<Id>(), size_t>()) validIndex() const noexcept {
			return m_tail.template validIndex<Id>() + m_head.validSize();
		}

		template <char Id>
		static constexpr decltype(std::enable_if_t<Head::template has<Id>(), size_t>()) validIndex(Flags flags) noexcept {
			return Head::template validIndex<Id>(flags);
		}

		template <char Id>
		static constexpr decltype(std::enable_if_t<!Head::template has<Id>(), size_t>()) validIndex(Flags flags) noexcept {
			return Tail::template validIndex<Id>(flags >> Head::size()) + Head::validSize(flags);
		}

		template <char Id>
		constexpr auto object(decltype(std::enable_if_t<Head::template has<Id>(), int>()) = 0) const noexcept {
			return m_head.template object<Id>();
		}

		template <char Id>
		constexpr auto object(decltype(std::enable_if_t<!Head::template has<Id>(), int>()) = 0) const noexcept {
			return m_tail.template object<Id>();
		}

		template <char Id>
		constexpr auto object(Container& container, decltype(std::enable_if_t<Head::template has<Id>(), int>()) = 0) const noexcept {
			return m_head.template object<Id>(container);
		}

		template <char Id>
		constexpr auto object(Container& container, decltype(std::enable_if_t<!Head::template has<Id>(), int>()) = 0) const noexcept {
			return m_tail.template object<Id>(container);
		}

		constexpr Head const& head() const noexcept {
			return m_head;
		}

		constexpr Tail const& tail() const noexcept {
			return m_tail;
		}
	};

	template <typename FreeObjects_, typename FreeObjects_::Flags flags_, bool PostponedBind>
	class BoundObjects {
	public:
		using FreeObjects = FreeObjects_;
		using Container = typename FreeObjects::Container;
		using BoundObject = typename FreeObjects::FreeObject::Bound_type;
		using type = typename BoundObject::type;
		using Ids = typename FreeObjects::Ids;
		enum { flags = flags_ };

	private:
		BoundObject m_objects[std::max<size_t>(1, FreeObjects::validSize(flags))] {};

		template <char Id>
		static size_t init(Container& container, FreeObjects const& fo, BoundObjects& bo) noexcept {
			size_t ix = fo.template validIndex<Id>(flags);
			if(fo.template valid<Id>()) {
				stored_assert(ix < sizeof(m_objects) / sizeof(m_objects[0]));
				bo.m_objects[ix] = fo.template object<Id>().apply_(container);
				stored_assert(bo.m_objects[ix].valid());
			}
			return ix;
		}

		template <char... Id>
		static BoundObjects create(FreeObjects const& fo, Container& container, ids<Id...>) noexcept {
			BoundObjects bo;
			size_t dummy[] = {0, init<Id>(container, fo, bo)...};
			(void)dummy;
			return bo;
		}

	public:
		static BoundObjects create(FreeObjects const& fo, Container& container) noexcept {
			return create(fo, container, Ids());
		}

		template <char Id>
		static constexpr bool has() noexcept {
			return FreeObjects::template has<Id>();
		}

		bool valid() const noexcept {
			return m_objects[0].valid();
		}

		template <char Id>
		static constexpr bool valid() noexcept {
			return FreeObjects::template valid<Id>(flags);
		}

		template <char Id>
		auto& get(std::enable_if_t<valid<Id>(), int> = 0) noexcept {
			return m_objects[FreeObjects::template validIndex<Id>(flags)];
		}

		template <char Id>
		auto const& get(std::enable_if_t<valid<Id>(), int> = 0) const noexcept {
			return m_objects[FreeObjects::template validIndex<Id>(flags)];
		}

		template <char Id>
		auto get(decltype(std::enable_if_t<!valid<Id>(), int>()) = 0) const noexcept {
			return BoundObject{};
		}
	};

	template <typename FreeObjects_, typename FreeObjects_::Flags flags_>
	class BoundObjects<FreeObjects_, flags_, true> {
	public:
		using FreeObjects = FreeObjects_;
		using Container = typename FreeObjects::Container;
		using FreeObject = typename FreeObjects::FreeObject;
		using BoundObject = typename FreeObjects::FreeObject::Bound_type;
		using type = typename BoundObject::type;
		using Ids = typename FreeObjects::Ids;
		enum { flags = flags_ };

	private:
		Container* m_container{};
		FreeObject m_objects[std::max<size_t>(1, FreeObjects::validSize(flags))];

		template <char Id>
		static constexpr size_t init(FreeObjects const& fo, BoundObjects& bo) noexcept {
			size_t ix = fo.template validIndex<Id>(flags);
			if(fo.template valid<Id>()) {
				stored_assert(ix < sizeof(m_objects) / sizeof(m_objects[0]));
				bo.m_objects[ix] = fo.template object<Id>();
				stored_assert(bo.m_objects[ix].valid());
			}
			return ix;
		}

		template <char... Id>
		static constexpr BoundObjects create(FreeObjects const& fo, Container& container, ids<Id...>) noexcept {
			BoundObjects bo;
			bo.m_container = &container;
			size_t dummy[] = {0, init<Id>(fo, bo)...};
			(void)dummy;
			return bo;
		}

	public:
		static constexpr BoundObjects create(FreeObjects const& fo, Container& container) noexcept {
			return create(fo, container, Ids());
		}

		template <char Id>
		static constexpr bool has() noexcept {
			return FreeObjects::template has<Id>();
		}

		constexpr bool valid() const noexcept {
			return m_container;
		}

		template <char Id>
		static constexpr bool valid() noexcept {
			return FreeObjects_::template valid<Id>(flags);
		}

		template <char Id>
		auto get(decltype(std::enable_if_t<valid<Id>(), int>()) = 0) const noexcept {
			stored_assert(m_container);
			return m_objects[FreeObjects::template validIndex<Id>(flags)].apply_(*m_container);
		}

		template <char Id>
		auto get(decltype(std::enable_if_t<!valid<Id>(), int>()) = 0) const noexcept {
			return BoundObject{};
		}
	};

	template <typename... B>
	class BoundObjectsList {};

	template <typename B0>
	class BoundObjectsList<B0> : public B0 {
	public:
		using Head = B0;
		using Tail = void;

		constexpr BoundObjectsList() noexcept = default;

		constexpr BoundObjectsList(Head&& head) noexcept
			: Head(std::move(head))
		{}

		constexpr BoundObjectsList(Head const& head) noexcept
			: Head(head)
		{}
	};

	template <typename B0, typename... B>
	class BoundObjectsList<B0,B...> {
	public:
		using Head = B0;
		using Tail = BoundObjectsList<B...>;
		using FreeObjects = typename Head::FreeObjects;
		using Container = typename Head::Container;
		using Ids = typename impl::merge_ids<typename Head::Ids, typename Tail::Ids>::type;
		enum { flags = Head::flags };

	private:
		Head m_head;
		Tail m_tail;

		constexpr BoundObjectsList(Head&& head, Tail&& tail) noexcept
			: m_head{std::move(head)}, m_tail{std::move(tail)}
		{}

	public:
		constexpr BoundObjectsList() noexcept = default;

		template <typename FreeObjectsList>
		static constexpr std::enable_if_t<std::is_same<BoundObjectsList, typename FreeObjectsList::template Bound<flags>>::value, BoundObjectsList>
		create(FreeObjectsList const& fo, Container& container) noexcept {
			return BoundObjectsList{
				Head::create(fo.head(), container),
				Tail::create(fo.tail(), container)
			};
		}

		template <char Id>
		static constexpr bool has() noexcept {
			return Head::template has<Id>() || Tail::template has<Id>();
		}

		bool valid() const noexcept {
			return m_head.valid();
		}

		template <char Id>
		static constexpr bool valid() noexcept {
			return Head::template valid<Id>() || Tail::template valid<Id>();
		}

		template <char Id>
		decltype(auto) get(decltype(std::enable_if_t<Head::template has<Id>(), int>()) = 0) const noexcept {
			return m_head.template get<Id>();
		}

		template <char Id>
		decltype(auto) get(std::enable_if_t<Head::template has<Id>(), int> = 0) noexcept {
			return m_head.template get<Id>();
		}

		template <char Id>
		decltype(auto) get(decltype(std::enable_if_t<!Head::template has<Id>(), int>()) = 0) const noexcept {
			return m_tail.template get<Id>();
		}

		template <char Id>
		decltype(auto) get(decltype(std::enable_if_t<!Head::template has<Id>(), int>()) = 0) noexcept {
			return m_tail.template get<Id>();
		}
	};

	template <typename Container, typename T = float>
	using AmplifierObjects = FreeObjectsList<
		FreeVariables<T, Container, 'I', 'g', 'o', 'l', 'h', 'F', 'O'>,
		FreeVariables<bool, Container, 'e'>>;

	/*!
	 * \brief An offset/gain amplifier, based on store variables.
	 *
	 * This class comes in very handy when converting ADC inputs to some
	 * SI-value. It includes an override field to force inputs to some test
	 * value.
	 *
	 * To use this class, add a scope to your store, like:
	 *
	 * \code
	 * {
	 *     float input
	 *     bool=true enable
	 *     float=2 gain
	 *     float=0.5 offset
	 *     float=-1 low
	 *     float=10 high
	 *     float=nan override
	 *     float output
	 * } amp
	 * \endcode
	 *
	 * All fields are optional.
	 *
	 * The amplifier basically does:
	 *
	 * \code
	 * if(override is nan)
	 *     output = min(high, max(low, input * gain + offset))
	 * else
	 *     output = override
	 * \endcode
	 *
	 * Then, instantiate the amplifier like this:
	 *
	 * \code
	 * // Construct a compile-time object, which resolves all fields in your store.
	 * constexpr auto amp_o = stored::Amplifier<stored::YourStore>::objects("/amp/");
	 * // Instantiate an Amplifier, tailored to the available fields in the store.
	 * stored::Amplifier<stored::YourStore, amp_o.flags()> amp{amp_o, yourStore};
	 * \endcode
	 *
	 * Calling \c amp() now uses the \c input and produces the a value in \c
	 * output.  Alternatively, or when the \c input field is absent in the
	 * store, call \c amp(x), where \c x is the input.
	 */
	template <typename Container, unsigned long long flags = 0, typename T = float>
	class Amplifier {
	public:
		using type = T;
		using Bound = typename AmplifierObjects<Container, type>::template Bound<flags>;

		constexpr Amplifier() noexcept = default;

		constexpr Amplifier(AmplifierObjects<Container,type> const& o, Container& container)
			: m_o{Bound::create(o, container)}
		{}

		template <char... OnlyId, size_t N>
		static constexpr auto objects(char const(&prefix)[N]) noexcept {
			return AmplifierObjects<Container, type>::template create<OnlyId...>(prefix,
				"input", "gain", "offset", "low", "high", "override", "output", "enable");
		}

		decltype(auto) inputObject() const noexcept { return m_o.template get<'I'>(); }
		decltype(auto) inputObject() noexcept { return m_o.template get<'I'>(); }
		type input() const noexcept { decltype(auto) o = inputObject(); return o.valid() ? o.get() : type(); }

		decltype(auto) gainObject() const noexcept { return m_o.template get<'g'>(); }
		decltype(auto) gainObject() noexcept { return m_o.template get<'g'>(); }
		type gain() const noexcept { decltype(auto) o = gainObject(); return o.valid() ? o.get() : type(1); }

		decltype(auto) offsetObject() const noexcept { return m_o.template get<'o'>(); }
		decltype(auto) offsetObject() noexcept { return m_o.template get<'o'>(); }
		type offset() const noexcept { decltype(auto) o = offsetObject(); return o.valid() ? o.get() : type(); }

		decltype(auto) lowObject() const noexcept { return m_o.template get<'l'>(); }
		decltype(auto) lowObject() noexcept { return m_o.template get<'l'>(); }
		type low() const noexcept {
			decltype(auto) o = lowObject();
			if(o.valid())
				return o.get();
			if(std::numeric_limits<type>::has_infinity)
				return -std::numeric_limits<type>::infinity();
			return std::numeric_limits<type>::lowest();
		}

		decltype(auto) highObject() const noexcept { return m_o.template get<'h'>(); }
		decltype(auto) highObject() noexcept { return m_o.template get<'h'>(); }
		type high() const noexcept {
			decltype(auto) o = highObject();
			if(o.valid())
				return o.get();
			if(std::numeric_limits<type>::has_infinity)
				return std::numeric_limits<type>::infinity();
			return std::numeric_limits<type>::max();
		}

		decltype(auto) overrideObject() const noexcept { return m_o.template get<'F'>(); }
		decltype(auto) overrideObject() noexcept { return m_o.template get<'F'>(); }
		type override_() const noexcept { decltype(auto) o = overrideObject(); return o.valid() ? o.get() : std::numeric_limits<type>::quiet_NaN(); }

		decltype(auto) outputObject() const noexcept { return m_o.template get<'O'>(); }
		decltype(auto) outputObject() noexcept { return m_o.template get<'O'>(); }
		type output() const noexcept { decltype(auto) o = outputObject(); return o.valid() ? o.get() : type(); }

		decltype(auto) enableObject() const noexcept { return m_o.template get<'e'>(); }
		decltype(auto) enableObject() noexcept { return m_o.template get<'e'>(); }
		bool enabled() const noexcept { decltype(auto) o = enableObject(); return !o.valid() || o.get(); }
		void enable(bool value) noexcept { decltype(auto) o = enableObject(); if(o.valid()) o = value; }
		void disable() noexcept { enable(false); }

		type operator()() noexcept {
			return run(input());
		}

		type operator()(type input) noexcept {
			decltype(auto) o = inputObject();
			if(o.valid())
				o = input;

			return run(input);
		}

	protected:
		type run(type input) noexcept {
			type output = override_();

			if(!std::isnan(output)) {
				// Keep override value.
			} else {
				if(!enabled())
					output = input;
				else if(std::isnan(output))
					output = input * gain() + offset();

				output = std::min(std::max(low(), output), high());
			}

			decltype(auto) oo = outputObject();
			if(oo.valid())
				oo = output;

			return output;
		}

	private:
		Bound m_o;
	};

	template <typename Container>
	using PinInObjects = FreeObjectsList<
		FreeFunctions<bool, Container, 'p'>,
		FreeVariables<int8_t, Container, 'F'>,
		FreeVariables<bool, Container, 'i'>>;

	/*!
	 * \brief An GPIO input pin, based on store variables.
	 *
	 * This class comes in very handy when a GPIO input should be observed and
	 * overridden while debugging.  It gives some interface between the
	 * hardware pin and the input that the application sees.
	 *
	 * To use this class, add a scope to your store, like:
	 *
	 * \code
	 * {
	 *     (bool) pin
	 *     int8=-1 override
	 *     bool input
	 *     (bool) get
	 * } pin
	 * \endcode
	 *
	 * All fields are optional. You can implement the store's \c pin function,
	 * override the virtual \c pin() function of the PinIn class, or pass the
	 * hardware pin value as an argument to the PinIn::operator().
	 *
	 * The pin basically does:
	 *
	 * \code
	 * switch(override) {
	 * case -1: input = pin; break;
	 * case  0: input = false; break;
	 * case  1: input = true; break;
	 * case  2: input = !pin; break;
	 * }
	 * \endcode
	 *
	 * Then, instantiate the pin like this:
	 *
	 * \code
	 * // Construct a compile-time object, which resolves all fields in your store.
	 * constexpr auto pin_o = stored::PinIn<stored::YourStore>::objects("/pin/");
	 * // Instantiate an PinIn, tailored to the available fields in the store.
	 * stored::PinIn<stored::YourStore, pin_o.flags()> pin{pin_o, yourStore};
	 * \endcode
	 *
	 * When \c pin() is called, it will invoke the \c pin function to get the
	 * actual hardware pin status.  Then, it will set the \c input variable.
	 *
	 * The \c get function is not used/provided by this \c PinIn. Implement
	 * this store function such that it calls and returns \c pin(). When
	 * applications read the \c get function, they will always get the actual
	 * pin value.
	 */
	template <typename Container, unsigned long long flags = 0>
	class PinIn {
	public:
		using Bound = typename PinInObjects<Container>::template Bound<flags>;

		constexpr PinIn() noexcept = default;

		constexpr PinIn(PinInObjects<Container> const& o, Container& container)
			: m_o{Bound::create(o, container)}
		{}

		template <char... OnlyId, size_t N>
		static constexpr auto objects(char const(&prefix)[N]) noexcept {
			return PinInObjects<Container>::template create<OnlyId...>(prefix,
				"pin", "override", "input");
		}

		decltype(auto) pinObject() const noexcept { return m_o.template get<'p'>(); }
		virtual bool pin() const noexcept { decltype(auto) o = pinObject(); return o.valid() ? o() : false; }

		decltype(auto) overrideObject() const noexcept { return m_o.template get<'F'>(); }
		decltype(auto) overrideObject() noexcept { return m_o.template get<'F'>(); }
		int8_t override_() const noexcept { decltype(auto) o = overrideObject(); return o.valid() ? o.get() : -1; }

		decltype(auto) inputObject() const noexcept { return m_o.template get<'i'>(); }
		decltype(auto) inputObject() noexcept { return m_o.template get<'i'>(); }
		bool input() const noexcept { decltype(auto) o = inputObject(); return o.valid() ? o.get() : (*this)(); }

		bool operator()() noexcept {
			return (*this)(pin());
		}

		bool operator()(bool pin) noexcept {
			bool i;
			switch(override_()) {
			default:
			case -1: i = pin; break;
			case 0: i = false; break;
			case 1: i = true; break;
			case 2: i = !pin; break;
			}

			decltype(auto) io = inputObject();
			if(io.valid())
				io = i;

			return i;
		}

	private:
		Bound m_o;
	};

	template <typename Container>
	using PinOutObjects = FreeObjectsList<
		FreeVariables<bool, Container, 'o'>,
		FreeFunctions<bool, Container, 'p'>>;

	/*!
	 * \brief An GPIO output pin, based on store variables.
	 *
	 * This class comes in very handy when a GPIO output should be observed and
	 * overridden while debugging.  It gives some interface between the
	 * hardware pin and the output that the application wants.
	 *
	 * To use this class, add a scope to your store, like:
	 *
	 * \code
	 * {
	 *     (bool) set
	 *     bool output
	 *     (int8) override
	 *     (bool) pin
	 * } pin
	 * \endcode
	 *
	 * All fields are optional, except \c output. You can implement the store's
	 * \c pin function, override the virtual \c pin() function of the PinOut
	 * class, or forward the return value of PinOut::operator() to the hardware
	 * pin.
	 *
	 * The pin basically does:
	 *
	 * \code
	 * switch(override) {
	 * case -1: pin = output; break;
	 * case  0: pin = false; break;
	 * case  1: pin = true; break;
	 * case  2: pin = !output; break;
	 * }
	 * \endcode
	 *
	 * Then, instantiate the pin like this:
	 *
	 * \code
	 * // Construct a compile-time object, which resolves all fields in your store.
	 * constexpr auto pin_o = stored::PinOut<stored::YourStore>::objects("/pin/");
	 * // Instantiate an PinOut, tailored to the available fields in the store.
	 * stored::PinOut<stored::YourStore, pin_o.flags()> pin{pin_o, yourStore};
	 * \endcode
	 *
	 * The \c set function is not used/provided by this \c PinOut. Implement
	 * this store function such that it calls \c pin() with the provided value.
	 * When applications write the \c set function, they will immediately
	 * control the hardware pin.
	 *
	 * Similar holds for the \c override function; implement it to call the
	 * #override_() of PinOut. This way, if one sets the override value, the
	 * hardware pin is updated accordingly.
	 */
	template <typename Container, unsigned long long flags = 0>
	class PinOut {
	public:
		using Bound = typename PinOutObjects<Container>::template Bound<flags>;

		constexpr PinOut() noexcept = default;

		constexpr PinOut(PinOutObjects<Container> const& o, Container& container)
			: m_o{Bound::create(o, container)}
		{
			static_assert(Bound::template valid<'o'>(), "'output' variable is mandatory");
		}

		template <char... OnlyId, size_t N>
		static constexpr auto objects(char const(&prefix)[N]) noexcept {
			return PinOutObjects<Container>::template create<OnlyId...>(prefix,
				"output", "pin");
		}

		decltype(auto) outputObject() const noexcept { return m_o.template get<'o'>(); }
		decltype(auto) outputObject() noexcept { return m_o.template get<'o'>(); }
		bool output() const noexcept { return outputObject().get(); }

		int8_t override_() const noexcept { return m_override; }
		void override_(int8_t x) noexcept { m_override = x; (*this)(); }

		decltype(auto) pinObject() noexcept { return m_o.template get<'p'>(); }
		virtual void pin(bool value) noexcept { decltype(auto) o = pinObject(); if(o.valid()) o(value); }

		bool operator()() noexcept {
			return (*this)(output());
		}

		bool operator()(bool output) noexcept {
			outputObject() = output;
			return run(output);
		}

	protected:
		bool run(bool output) noexcept {
			bool p;
			switch(override_()) {
			default:
			case -1: p = output; break;
			case 0: p = false; break;
			case 1: p = true; break;
			case 2: p = !output; break;
			}

			pin(p);
			return p;
		}

	private:
		Bound m_o;
		int8_t m_override{-1};
	};

	template <typename Container, typename T=float>
	using PIDObjects = FreeObjectsList<
		FreeFunctions<float, Container, 'f'>,
		FreeVariables<T, Container, 'y', 's', 'p', 'i', 'd', 'k', 'I', 'L', 'H', 'l', 'h', 'F', 'u'>,
		FreeVariables<bool, Container, 'e', 'r'>>;

	/*!
	 * \brief PID controller, based on store variables.
	 *
	 * To use this class, add a scope to your store, like:
	 *
	 * \code
	 * {
	 *     (float) frequency (Hz)
	 *     float y
	 *     float setpoint
	 *     bool=true enable
	 *     float=1 Kp
	 *     float=inf Ti (s)
	 *     float=0 Td (s)
	 *     float=0 Kff
	 *     float int
	 *     float=-inf int low
	 *     float=inf int high
	 *     float=-inf low
	 *     float=inf high
	 *     bool reset
	 *     float=nan override
	 *     float u
	 * } pid
	 * \endcode
	 *
	 * Only \c frequency, \c setpoint, and \c Kp are mandatory.
	 * Then, instantiate the controller like this:
	 *
	 * \code
	 * // Construct a compile-time object, which resolves all fields in your store.
	 * constexpr auto pid_o = stored::PID<stored::YourStore>::objects("/pid/");
	 * // Instantiate an PID, tailored to the available fields in the store.
	 * stored::PID<stored::YourStore, pid_o.flags()> pid{pid_o, yourStore};
	 * \endcode
	 */
	template <typename Container, unsigned long long flags = 0, typename T = float>
	class PID {
	public:
		using type = T;
		using Bound = typename PIDObjects<Container,type>::template Bound<flags>;

		constexpr PID() noexcept = default;

		constexpr PID(PIDObjects<Container,type> const& o, Container& container)
			: m_o{Bound::create(o, container)}
		{
			static_assert(Bound::template valid<'f'>(), "'frequency' function is mandatory");
			static_assert(Bound::template valid<'s'>(), "'setpoint' variable is mandatory");
			static_assert(Bound::template valid<'p'>(), "'Kp' variable is mandatory");

			decltype(auto) uo = uObject();
			if(uo.valid())
				m_u = uo.get();
			else
				m_u = std::max<type>(low(), 0);
		}

		template <char... OnlyId, size_t N>
		static constexpr auto objects(char const(&prefix)[N]) noexcept {
			return PIDObjects<Container,type>::template create<OnlyId...>(prefix,
				"frequency", "y", "setpoint", "Kp", "Ti", "Td", "Kff", "int",
				"int low", "int high", "low", "high", "override", "u", "enable", "reset");
		}

		decltype(auto) frequencyObject() const noexcept { return m_o.template get<'f'>(); }
		float frequency() const noexcept { return frequencyObject()(); }

		decltype(auto) yObject() const noexcept { return m_o.template get<'y'>(); }
		decltype(auto) yObject() noexcept { return m_o.template get<'y'>(); }
		type y() const noexcept { decltype(auto) o = yObject(); return o.valid() ? o.get() : type(); }

		decltype(auto) setpointObject() const noexcept { return m_o.template get<'s'>(); }
		type setpoint() const noexcept { return setpointObject().get(); }

		decltype(auto) KpObject() const noexcept { return m_o.template get<'p'>(); }
		type Kp() const noexcept { return KpObject().get(); }

		decltype(auto) TiObject() const noexcept { return m_o.template get<'i'>(); }
		type Ti() const noexcept { decltype(auto) o = TiObject(); return o.valid() ? o.get() : std::numeric_limits<type>::infinity(); }
		type Ki() const noexcept { return m_Ki; }

		decltype(auto) TdObject() const noexcept { return m_o.template get<'d'>(); }
		type Td() const noexcept { decltype(auto) o = TdObject(); return o.valid() ? o.get() : (type)0; }
		type Kd() const noexcept { return m_Kd; }

		decltype(auto) KffObject() const noexcept { return m_o.template get<'K'>(); }
		type Kff() const noexcept { decltype(auto) o = KffObject(); return o.valid() ? o.get() : (type)0; }

		decltype(auto) intObject() const noexcept { return m_o.template get<'I'>(); }
		decltype(auto) intObject() noexcept { return m_o.template get<'I'>(); }
		type int_() const noexcept { m_int; }

		decltype(auto) intLowObject() const noexcept { return m_o.template get<'L'>(); }
		type intLow() const noexcept { decltype(auto) o = intLowObject(); return o.valid() ? o.get() : -std::numeric_limits<type>::infinity(); }

		decltype(auto) intHighObject() const noexcept { return m_o.template get<'H'>(); }
		type intHigh() const noexcept { decltype(auto) o = intHighObject(); return o.valid() ? o.get() : std::numeric_limits<type>::infinity(); }

		decltype(auto) lowObject() const noexcept { return m_o.template get<'l'>(); }
		type low() const noexcept { decltype(auto) o = lowObject(); return o.valid() ? o.get() : -std::numeric_limits<type>::infinity(); }

		decltype(auto) highObject() const noexcept { return m_o.template get<'h'>(); }
		type high() const noexcept { decltype(auto) o = highObject(); return o.valid() ? o.get() : std::numeric_limits<type>::infinity(); }

		decltype(auto) overrideObject() const noexcept { return m_o.template get<'F'>(); }
		type override_() const noexcept { decltype(auto) o = overrideObject(); return o.valid() ? o.get() : std::numeric_limits<type>::quiet_NaN(); }

		decltype(auto) uObject() const noexcept { return m_o.template get<'u'>(); }
		decltype(auto) uObject() noexcept { return m_o.template get<'u'>(); }
		type u() const noexcept { return m_u; }

		decltype(auto) enableObject() const noexcept { return m_o.template get<'e'>(); }
		bool enabled() const noexcept { decltype(auto) o = enableObject(); return !o.valid() || o.get(); }

		decltype(auto) resetObject() const noexcept { return m_o.template get<'r'>(); }
		decltype(auto) resetObject() noexcept { return m_o.template get<'r'>(); }
		bool reset() const noexcept { decltype(auto) o = resetObject(); return !o.valid() || o.get(); }

		type operator()(type y) {
			decltype(auto) o = yObject();
			if(o.valid())
				o = y;

			return run(y);
		}

		type operator()() {
			return run(y());
		}

	protected:
		type run(type y) {
			if(!enabled())
				return m_u;

			type u = override_();

			if(likely(std::isnan(u))) {
				bool doReset = false;
				decltype(auto) reset_o = resetObject();
				if(reset_o.valid()) {
					if(unlikely(reset_o.get())) {
						doReset = true;
						reset_o = false;
					}
				} else if(unlikely(std::isnan(m_y_prev))) {
					doReset = true;
				}

				type sp = setpoint();
				type e = sp - y;

				if(unlikely(doReset)) {
					float f = frequency();
					m_Ki = 0;
					m_Kd = 0;
					m_y_prev = y;

					if(!std::isnan(f) && f > 0) {
						float dt = 1.0f / frequency();
						if(Ti() != 0)
							m_Ki = Kp() * dt / Ti();
						m_Kd = -Kp() * Td() / dt;
					}
				}

				u = Kp() * e + m_int + Kff() * sp;

				type di = Ki() * e;
				if((u >= low() || di > 0) && (u <= high() || di < 0)) {
					// Anti-windup: only update m_int when we are within output
					// bounds, or if we get back into those bounds.
					type i = std::max(intLow(), std::min(intHigh(), m_int + di));
					u += i - m_int;
					m_int = i;

					decltype(auto) io = intObject();
					if(io.valid())
						io = m_int;
				}

				if(Kd() != 0) {
					u += Kd() * (y - m_y_prev);
					m_y_prev = y;
				}

				u = std::max(low(), std::min(high(), u));
			}

			decltype(auto) uo = uObject();
			if(uo.valid())
				uo = u;

			return u;
		}

	private:
		Bound m_o;
		type m_y_prev{std::numeric_limits<type>::quiet_NaN()};
		type m_Ki{};
		type m_Kd{};
		type m_int{};
		type m_u{};
	};

} // namespace

#endif // C++14
#endif // __cplusplus
#endif // LIBSTORED_COMPONENTS_H
