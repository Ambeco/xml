#pragma once
#include <memory>
#include <optional>
#include <utility>

namespace mpd {
	/**
	* A single element local container like std::optional, but with type erasure, like std::unique_ptr
	* So you can put derived classes in it.
	* Requires base_t to have a virtual destructor. 
	* In order to copy construct or copy assign a type_erased, base_t must also have this method:
	* virtual base_t* copy_construct_at(char* buffer, std::size_t buffer_size)const&;
	* and each most-derived class must implement with: {return new(buffer)DerivedT(*this);}
	* In order to move construct, move assign, or swap a type_erased, base_t must also have this method:
	* virtual base_t* move_construct_at(char* buffer, std::size_t buffer_size)& noexcept;
	* and each most-derived class must implement with: {return new(buffer)DerivedT(std::move(*this));}
	* (Technically the move can throw, but why would you do that?)
	* Unlike std::optional, only ==nullptr and !=nullptr comparison operators are implemented, since 
	* that rarely makes since with derived classes.
	* This also enforces that the buf_size template parameter is >= sizeof(DerivedT)
	*/
	template<class base_t, std::size_t buf_size = 8*sizeof(void*), std::size_t align = alignof(void*)>
	class type_erased {
		base_t* data;
		typename std::aligned_storage<buf_size, align>::type buffer;
		void verify() const { if (data == nullptr) throw std::bad_optional_access("dereferencing null type_erased"); }
		char* get_buffer() { return reinterpret_cast<char*>(&buffer); }
		void copy_construct_from(const type_erased& r) noexcept(noexcept(data->copy_construct_at(std::declval<char*>(), std::size_t{}))) {
			reset();
			if (r.data == nullptr) { data = nullptr; }
			else { data = r.data->copy_construct_at(get_buffer(), buf_size); }
		}
		void move_construct_from(type_erased& r) noexcept(noexcept(r.data->move_construct_at(std::declval<char*>(), std::size_t{}))) {
			reset();
			if (r.data == nullptr) { data = nullptr; }
			else { data = std::move(r.data)->move_construct_at(get_buffer(), buf_size); }
		}
	public:
		static_assert(std::has_virtual_destructor<base_t>::value, "Base class must have a virtual destructor");
		type_erased() noexcept :data(nullptr) {}
		type_erased(std::nullopt_t) noexcept :data(nullptr) {}
		type_erased(const type_erased& r) noexcept (noexcept(copy_construct_from(r)))
		:data(nullptr) { copy_construct_from(r); }
		type_erased(type_erased&& r) noexcept (noexcept(move_construct_from(r)))
		:data(nullptr) { move_construct_from(r); }
		template<class DerivedT, class = std::enable_if_t<std::is_base_of<base_t, DerivedT>::value> >
		type_erased(DerivedT&& r) noexcept(noexcept(emplace(std::in_place_type_t<DerivedT>{}, std::move(r))))
		:data(nullptr) { emplace(std::in_place_type_t<DerivedT>{}, std::move(r)); }
		template<class DerivedT, class...Us>
		type_erased(std::in_place_type_t<DerivedT> type, Us&&... us) noexcept(noexcept(emplace(type,std::forward<Us>(us)...)))
		:data(nullptr) { emplace(type, std::forward<Us>(us)...); }
		~type_erased() noexcept { reset(); }
		type_erased& operator=(std::nullopt_t) noexcept { reset(); return *this; }
		type_erased& operator=(const type_erased& r) noexcept(noexcept(copy_construct_from(r)))
		{ reset(); copy_construct_from(r); return *this; }
		type_erased& operator=(type_erased&& r) noexcept(noexcept(move_construct_from(r)))
		{ reset(); move_construct_from(r); return *this; }
		template<class DerivedT, class = std::enable_if_t<std::is_base_of<base_t, DerivedT>::value> >
		type_erased& operator=(DerivedT&& r) noexcept(noexcept(emplace(std::in_place_type_t<DerivedT>{}, std::move(r))))
		{ emplace(std::in_place_type_t<DerivedT>{}, std::move(r)); return *this; }
		void reset() noexcept { if (data) { data->~base_t(); data = nullptr; } }
		template<class DerivedT, class...Us>
		void emplace(std::in_place_type_t<DerivedT>, Us&&... us) noexcept(noexcept(new(get_buffer())DerivedT(std::forward<Us>(us)...)))
		{ static_assert(sizeof(DerivedT) <= buf_size); reset(); data = new(get_buffer())DerivedT(std::forward<Us>(us)...); }
		template<class DerivedT, class...Us, class = std::enable_if_t<std::is_constructible<base_t, Us...>::value> >
		void emplace(Us&&...params) noexcept(noexcept(emplace(std::in_place_type_t<DerivedT>{}, std::forward<Us>(params)...)))
		{ emplace(std::in_place_type_t<DerivedT>{}, std::forward<Us>(params)...); }
		void swap(type_erased& r) noexcept(noexcept(move_construct_from(r)))
		{ type_erased t(std::move(*this)); move_construct_from(r); r.move_construct_from(t); }
		base_t& value() & { verify();  return *data; }
		const base_t& value() const& { verify();  return *data; }
		base_t&& value() && { verify();  return std::move(*data); }
		base_t& operator*() & noexcept { return *data; }
		const base_t& operator*() const& noexcept { return *data; }
		base_t&& operator*()&& noexcept { return std::move(*data); }
		base_t* operator->() noexcept { return data; }
		const base_t* operator->() const noexcept { return data; }
		template<class DerivedT> const base_t& value_or(const DerivedT& default_value) const&
		{return data != nullptr ? *data : value; }
		template<class DerivedT> base_t&& value_or(DerivedT&& default_value) &&
		{return data != nullptr ? std::move(*data) : std::move(value); }
		explicit operator bool() const noexcept { return data != nullptr; }
		bool has_value() const noexcept { return data != nullptr; }
	}; 
	template<class base_t, std::size_t buf_size, std::size_t align>
	bool operator==(type_erased<base_t, buf_size, align>& item, std::nullopt_t)
	{ return !item.has_value() }
	template<class base_t, std::size_t buf_size, std::size_t align>
	bool operator!=(type_erased<base_t, buf_size, align>& item, std::nullopt_t)
	{ return item.has_value() }
	template<class base_t, std::size_t buf_size, std::size_t align>
	bool operator==(std::nullopt_t, type_erased<base_t, buf_size, align>& item)
	{ return !item.has_value() }
	template<class base_t, std::size_t buf_size, std::size_t align>
	bool operator!=(std::nullopt_t, type_erased<base_t, buf_size, align>& item)
	{ return item.has_value() }
}