/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/internal/script/lua.h>
#include <mgba/script.h>

#include "script/test.h"

#define SETUP_LUA \
	struct mScriptContext context; \
	mScriptContextInit(&context); \
	struct mScriptEngineContext* lua = mScriptContextRegisterEngine(&context, mSCRIPT_ENGINE_LUA); \
	mScriptContextAttachStdlib(&context)

M_TEST_SUITE_SETUP(mScriptStdlib) {
	if (mSCRIPT_ENGINE_LUA->init) {
		mSCRIPT_ENGINE_LUA->init(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_SUITE_TEARDOWN(mScriptStdlib) {
	if (mSCRIPT_ENGINE_LUA->deinit) {
		mSCRIPT_ENGINE_LUA->deinit(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_DEFINE(bitMask) {
	SETUP_LUA;

	TEST_PROGRAM("assert(util)");
	TEST_PROGRAM("assert(util.makeBitmask)");
	TEST_PROGRAM("assert(util.makeBitmask{0} == 1)");
	TEST_PROGRAM("assert(util.makeBitmask{1} == 2)");
	TEST_PROGRAM("assert(util.makeBitmask{0, 1} == 3)");
	TEST_PROGRAM("assert(util.makeBitmask{1, 1} == 2)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(bitUnmask) {
	SETUP_LUA;

	TEST_PROGRAM("assert(util)");
	TEST_PROGRAM("assert(util.expandBitmask)");
	TEST_PROGRAM("assert(#util.expandBitmask(0) == 0)");
	TEST_PROGRAM("assert(#util.expandBitmask(1) == 1)");
	TEST_PROGRAM("assert(util.expandBitmask(1)[1] == 0)");
	TEST_PROGRAM("assert(#util.expandBitmask(2) == 1)");
	TEST_PROGRAM("assert(util.expandBitmask(2)[1] == 1)");
	TEST_PROGRAM("assert(#util.expandBitmask(3) == 2)");
	TEST_PROGRAM("assert(util.expandBitmask(3)[1] == 0 or util.expandBitmask(3)[1] == 1)");
	TEST_PROGRAM("assert(util.expandBitmask(3)[2] == 0 or util.expandBitmask(3)[2] == 1)");
	TEST_PROGRAM("assert(#util.expandBitmask(6) == 2)");
	TEST_PROGRAM("assert(util.expandBitmask(6)[1] == 1 or util.expandBitmask(6)[1] == 2)");
	TEST_PROGRAM("assert(util.expandBitmask(6)[2] == 1 or util.expandBitmask(6)[2] == 2)");
	TEST_PROGRAM("assert(#util.expandBitmask(7) == 3)");
	TEST_PROGRAM("assert(#util.expandBitmask(11) == 3)");
	TEST_PROGRAM("assert(#util.expandBitmask(15) == 4)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(callbacks) {
	SETUP_LUA;

	TEST_PROGRAM(
		"val = 0\n"
		"function cb()\n"
		"	val = val + 1\n"
		"end\n"
		"id = callbacks:add('test', cb)\n"
		"assert(id)"
	);

	TEST_VALUE(S32, "val", 0);

	mScriptContextTriggerCallback(&context, "test", NULL);
	TEST_VALUE(S32, "val", 1);

	mScriptContextTriggerCallback(&context, "test", NULL);
	TEST_VALUE(S32, "val", 2);

	TEST_PROGRAM("callbacks:remove(id)");

	mScriptContextTriggerCallback(&context, "test", NULL);
	TEST_VALUE(S32, "val", 2);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(oneshot) {
	SETUP_LUA;

	TEST_PROGRAM(
		"val = 0\n"
		"function cb()\n"
		"	val = val + 1\n"
		"end\n"
		"id = callbacks:oneshot('test', cb)\n"
		"assert(id)"
	);

	TEST_VALUE(S32, "val", 0);

	mScriptContextTriggerCallback(&context, "test", NULL);
	TEST_VALUE(S32, "val", 1);

	mScriptContextTriggerCallback(&context, "test", NULL);
	TEST_VALUE(S32, "val", 1);

	TEST_PROGRAM(
		"id = callbacks:oneshot('test', cb)\n"
		"assert(id)\n"
		"callbacks:remove(id)"
	);

	mScriptContextTriggerCallback(&context, "test", NULL);
	TEST_VALUE(S32, "val", 1);

	mScriptContextDeinit(&context);
}

static void _tableIncrement(struct mScriptValue* table) {
	assert_non_null(table);
	struct mScriptValue* value = mScriptTableLookup(table, &mSCRIPT_MAKE_CHARP("key"));
	assert_non_null(value);
	assert_ptr_equal(value->type, mSCRIPT_TYPE_MS_S32);
	++value->value.s32;
}

mSCRIPT_BIND_VOID_FUNCTION(tableIncrement, _tableIncrement, 1, WTABLE, table);

M_TEST_DEFINE(callbackWeakref) {
	SETUP_LUA;

	struct mScriptValue* table = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	struct mScriptList args;
	mScriptListInit(&args, 1);
	mScriptValueWrap(table, mScriptListAppend(&args));
	struct mScriptValue* lambda = mScriptLambdaCreate0(&tableIncrement, &args);
	mScriptListDeinit(&args);
	struct mScriptValue* weakref = mScriptContextMakeWeakref(&context, lambda);
	mScriptContextAddCallback(&context, "test", weakref);

	struct mScriptValue* key = mScriptStringCreateFromUTF8("key");
	struct mScriptValue* value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S32);
	value->value.s32 = 1;
	mScriptTableInsert(table, key, value);

	mScriptContextTriggerCallback(&context, "test", NULL);
	assert_int_equal(value->value.s32, 2);

	mScriptContextClearWeakref(&context, weakref->value.u32);
	mScriptValueDeref(weakref);

	mScriptContextTriggerCallback(&context, "test", NULL);
	assert_int_equal(value->value.s32, 2);

	mScriptValueDeref(table);
	mScriptValueDeref(key);
	mScriptValueDeref(value);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(rectangle) {
	SETUP_LUA;

	struct mRectangle rect = {
		.x = 1,
		.y = 2,
		.width = 3,
		.height = 4,
	};
	struct mScriptValue rval = mSCRIPT_MAKE_S(mRectangle, &rect);
	mScriptContextSetGlobal(&context, "testRect", &rval);
	TEST_PROGRAM("assert(testRect)");
	TEST_PROGRAM("assert(testRect.x == 1)");
	TEST_PROGRAM("assert(testRect.y == 2)");
	TEST_PROGRAM("assert(testRect.width == 3)");
	TEST_PROGRAM("assert(testRect.height == 4)");
	TEST_PROGRAM(
		"testRect.x = 10\n"
		"testRect.y = 11\n"
		"testRect.width = 20\n"
		"testRect.height = 21\n"
	);
	assert_int_equal(rect.x, 10);
	assert_int_equal(rect.y, 11);
	assert_int_equal(rect.width, 20);
	assert_int_equal(rect.height, 21);

	TEST_PROGRAM("newRect = util.newRectangle(5, 6, 10, 11)");
	TEST_PROGRAM("assert(newRect.x == 5)");
	TEST_PROGRAM("assert(newRect.y == 6)");
	TEST_PROGRAM("assert(newRect.width == 10)");
	TEST_PROGRAM("assert(newRect.height == 11)");

	TEST_PROGRAM("testRect2 = testRect:copy()");
	TEST_PROGRAM("assert(testRect2.x == 10)");
	TEST_PROGRAM("assert(testRect2.y == 11)");
	TEST_PROGRAM("assert(testRect2.width == 20)");
	TEST_PROGRAM("assert(testRect2.height == 21)");

	TEST_PROGRAM("assert(testRect2:intersection(newRect))");
	TEST_PROGRAM("assert(testRect2.x == 10)");
	TEST_PROGRAM("assert(testRect2.y == 11)");
	TEST_PROGRAM("assert(testRect2.width == 5)");
	TEST_PROGRAM("assert(testRect2.height == 6)");

	TEST_PROGRAM("testRect2:union(testRect)");
	TEST_PROGRAM("assert(testRect2.x == 10)");
	TEST_PROGRAM("assert(testRect2.y == 11)");
	TEST_PROGRAM("assert(testRect2.width == 20)");
	TEST_PROGRAM("assert(testRect2.height == 21)");

	TEST_PROGRAM("testSize = testRect:size()");
	TEST_PROGRAM("assert(testRect.width == testSize.width)");
	TEST_PROGRAM("assert(testRect.height == testSize.height)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(size) {
	SETUP_LUA;

	struct mSize size = {
		.width = 1,
		.height = 2,
	};
	struct mScriptValue sval = mSCRIPT_MAKE_S(mSize, &size);
	mScriptContextSetGlobal(&context, "testSize", &sval);
	TEST_PROGRAM("assert(testSize)");
	TEST_PROGRAM("assert(testSize.width == 1)");
	TEST_PROGRAM("assert(testSize.height == 2)");
	TEST_PROGRAM(
		"testSize.width = 3\n"
		"testSize.height = 4\n"
	);
	assert_int_equal(size.width, 3);
	assert_int_equal(size.height, 4);

	TEST_PROGRAM("newSize = util.newSize(5, 6)");
	TEST_PROGRAM("assert(newSize.width == 5)");
	TEST_PROGRAM("assert(newSize.height == 6)");

	TEST_PROGRAM("testSize2 = testSize:copy()");
	TEST_PROGRAM("assert(testSize2.width == 3)");
	TEST_PROGRAM("assert(testSize2.height == 4)");

	mScriptContextDeinit(&context);
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(mScriptStdlib,
	cmocka_unit_test(bitMask),
	cmocka_unit_test(bitUnmask),
	cmocka_unit_test(callbacks),
	cmocka_unit_test(oneshot),
	cmocka_unit_test(callbackWeakref),
	cmocka_unit_test(rectangle),
	cmocka_unit_test(size),
)
