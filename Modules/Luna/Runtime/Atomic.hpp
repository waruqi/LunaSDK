/*!
* This file is a portion of Luna SDK.
* For conditions of distribution and use, see the disclaimer
* and license in LICENSE.txt
* 
* @file Atomic.hpp
* @author JXMaster
* @date 2018/12/7
 */
#pragma once
#include "PlatformDefines.hpp"

#if defined(LUNA_PLATFORM_WINDOWS)
#include "Source/Platform/Windows/AtomicImpl.hpp"
#elif defined(LUNA_PLATFORM_POSIX)
#include "Source/Platform/POSIX/AtomicImpl.hpp"
#endif

namespace Luna
{
	//! Atomically reads one value from the variable.
	//! The read operation is atomic by default, this only prevents the compiler to optimize the variable.
	inline i32 atom_read_i32(const i32 volatile* v) { return *v; }
	inline u32 atom_read_u32(const u32 volatile* v) { return *v; }
	inline i64 atom_read_i64(const i64 volatile* v) { return *v; }
	inline u64 atom_read_u64(const u64 volatile* v) { return *v; }
	inline usize atom_read_usize(const usize volatile* v) { return *v; }

	//! Atomically writes to one value.
	//! The write operation is atomic by default, this only prevents the compiler to optimize the variable.
	inline void atom_store_i32(i32 volatile* dst, i32 v) { *dst = v; }
	inline void atom_store_u32(u32 volatile* dst, u32 v) { *dst = v; }
	inline void atom_store_i64(i64 volatile* dst, i64 v) { *dst = v; }
	inline void atom_store_u64(u64 volatile* dst, u64 v) { *dst = v; }
	inline void atom_store_usize(usize volatile* dst, usize v) { *dst = v; }

	//! Atomically increase the value of the variable by 1. This operation cannot be interrupted by system thread switching.
	//! @param[in] value The pointer to the variable that needs to be changed.
	//! @return Returns the value of the variable after this operation.
	inline i32 atom_inc_i32(i32 volatile* v);
	inline u32 atom_inc_u32(u32 volatile* v);
	inline i64 atom_inc_i64(i64 volatile* v);
	inline u64 atom_inc_u64(u64 volatile* v);
	inline usize atom_inc_usize(usize volatile* v);

	//! Atomically decrease the value of the variable by 1. This operation cannot be interrupted by system thread switching.
	//! @param[in] value The pointer to the variable that needs to be changed.
	//! @return Returns the value of the variable after this operation.
	inline i32 atom_dec_i32(i32 volatile* v);
	inline u32 atom_dec_u32(u32 volatile* v);
	inline i64 atom_dec_i64(i64 volatile* v);
	inline u64 atom_dec_u64(u64 volatile* v);
	inline usize atom_dec_usize(usize volatile* v);

	//! Atomically increase the value of the variable by the the value provided. This operation cannot be interrupted by system thread switching.
	//! @param[in] base The pointer to the variable that needs to be changed.
	//! @param[in] value The value that needs to be added to the variable.
	//! @return Returns the value of the variable before this operation.
	inline i32 atom_add_i32(i32 volatile* base, i32 v);
	inline u32 atom_add_u32(u32 volatile* base, i32 v);
	inline i64 atom_add_i64(i64 volatile* base, i64 v);
	inline u64 atom_add_u64(u64 volatile* base, i64 v);
	inline usize atom_add_usize(usize volatile* base, isize v);

	//! Atomically replace the value of the variable with the value provided. This operation cannot be interrupted by system thread switching.
	//! @param[in] base The pointer to the variable that needs to be changed.
	//! @param[in] value The value that needs to be set to the variable.
	//! @return Returns the value of the variable before this operation took place.
	inline i32 atom_exchange_i32(i32 volatile* dst, i32 v);
	inline u32 atom_exchange_u32(u32 volatile* dst, u32 v);
	inline i64 atom_exchange_i64(i64 volatile* dst, i64 v);
	inline u64 atom_exchange_u64(u64 volatile* dst, u64 v);
	template <typename _Ty>
	inline _Ty* atom_exchange_pointer(_Ty* volatile* target, void* value);
	inline usize atom_exchange_usize(usize volatile* dst, usize v);

	//! Atomically compare the value of the variable with the value provided by `comperand`. This operation cannot be interrupted by system thread switching.
	//! If the value of the variable equals to the value provided by `comperand`, the value of the variable will be replaced by the value provided by `exchange`, and the old value of the variable will be returned.
	//! If the value of the variable does not equal to the value provided by `comperand`, the value of the variable will not change, and this value it will be returned.
	inline i32 atom_compare_exchange_i32(i32 volatile* dst, i32 exchange, i32 comperand);
	inline u32 atom_compare_exchange_u32(u32 volatile* dst, u32 exchange, u32 comperand);
	template <typename _Ty>
	inline _Ty* atom_compare_exchange_pointer(_Ty* volatile* dst, void* exchange, void* comperand);
#ifdef LUNA_PLATFORM_64BIT
	inline i64 atom_compare_exchange_i64(i64 volatile* dst, i64 exchange, i64 comperand);
	inline u64 atom_compare_exchange_u64(u64 volatile* dst, u64 exchange, u64 comperand);
#endif
	inline usize atom_compare_exchange_usize(usize volatile* dst, usize exchange, usize comperand);
}