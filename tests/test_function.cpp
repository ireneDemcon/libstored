/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "TestStore.h"
#include "gtest/gtest.h"

namespace {

class FunctionTestStore : public STORE_BASE_CLASS(TestStoreBase, FunctionTestStore) {
	STORE_CLASS_BODY(TestStoreBase, FunctionTestStore)
public:
	FunctionTestStore()
		: m_f_read__write(4)
	{}

	void __f_read__write(bool set, double& value)
	{
		if(set)
			m_f_read__write = value;
		else
			value = m_f_read__write;
	}

	void __f_read_only(bool set, uint16_t& value)
	{
		if(!set)
			value = saturated_cast<uint16_t>(m_f_read__write);
	}

	size_t __f_write_only(bool set, char* buffer, size_t len)
	{
		if(!set)
			return 0;

		printf("f write-only: %.*s\n", (int)len, buffer);
		return len;
	}

	void __array_f_int_0(bool set, int32_t& value)
	{
		if(!set)
			value = (int32_t)0;
	}
	void __array_f_int_1(bool set, int32_t& value)
	{
		if(!set)
			value = (int32_t)0;
	}
	void __array_f_int_2(bool set, int32_t& value)
	{
		if(!set)
			value = (int32_t)0;
	}
	void __array_f_int_3(bool set, int32_t& value)
	{
		if(!set)
			value = (int32_t)0;
	}
	size_t __array_f_blob_0(bool set, void* value, size_t len)
	{
		UNUSED(set)
		UNUSED(value)
		UNUSED(len)
		return 0;
	}
	size_t __array_f_blob_1(bool set, void* value, size_t len)
	{
		UNUSED(set)
		UNUSED(value)
		UNUSED(len)
		return 0;
	}

private:
	double m_f_read__write;
};

TEST(Function, ReadWrite)
{
	FunctionTestStore store;
	EXPECT_DOUBLE_EQ(store.f_read__write(), 4.0);
	store.f_read__write(5.0);
	EXPECT_DOUBLE_EQ(store.f_read__write(), 5.0);
}

TEST(Function, ReadOnly)
{
	FunctionTestStore store;
	EXPECT_EQ(store.f_read_only(), 4u);
	store.f_read__write(5.6);
	EXPECT_EQ(store.f_read_only(), 6u);
}

TEST(Function, WriteOnly)
{
	FunctionTestStore store;
	char buffer[] = "hi all!";
	EXPECT_EQ(store.f_write_only.get(buffer, sizeof(buffer)), 0);
	EXPECT_EQ(store.f_write_only.set(buffer, strlen(buffer)), 4u);
}

TEST(Function, FreeFunction)
{
	FunctionTestStore store;

	constexpr auto rw = FunctionTestStore::freeFunction<double>("/f read/write");
	static_assert(rw.valid(), "");

	rw.apply(store) = 123.4;
	EXPECT_DOUBLE_EQ(store.f_read__write.get(), 123.4);

	store.f_read__write = 56.7;
	constexpr auto ro = FunctionTestStore::freeFunction<uint16_t>("/f read-only");
	EXPECT_EQ(ro.apply(store).get(), 57);
}

} // namespace
