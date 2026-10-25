/* Standalone tests for the duktape compatibility layer */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "javascript/quickjs/duk_compat.h"

static int failures;
static int checks;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
	printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* stubs for netsurf */
void nslog_log(const char *file, const char *func, int ln, const char *format, ...)
{
	va_list ap;
	printf("  log: ");
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
	printf("\n");
}

static JSClassID thing_class;
static int thing_priv = 42;

void *qjsduk_lookup_private(duk_context *ctx, JSValueConst obj)
{
	return JS_GetOpaque(obj, thing_class);
}

#define PRIVATE "\xFF\xFFNETSURF_DUKTAPE_PRIVATE"
#define HIDDEN "\xFF\xFFNETSURF_DUKTAPE_HIDDEN"

static JSValue eval(JSContext *jctx, const char *src)
{
	return JS_Eval(jctx, src, strlen(src), "test", JS_EVAL_TYPE_GLOBAL);
}

/* evaluate and return string result in buf */
static const char *eval_str(JSContext *jctx, const char *src)
{
	static char buf[512];
	JSValue v = eval(jctx, src);
	const char *s;
	if (JS_IsException(v)) {
		JSValue e = JS_GetException(jctx);
		s = JS_ToCString(jctx, e);
		snprintf(buf, sizeof buf, "EXC:%s", s ? s : "?");
		JS_FreeCString(jctx, s);
		JS_FreeValue(jctx, e);
		return buf;
	}
	s = JS_ToCString(jctx, v);
	snprintf(buf, sizeof buf, "%s", s ? s : "?");
	JS_FreeCString(jctx, s);
	JS_FreeValue(jctx, v);
	return buf;
}

static duk_ret_t add2(duk_context *ctx)
{
	CHECK(duk_get_top(ctx) == 2);
	duk_push_number(ctx, duk_to_int(ctx, 0) + duk_to_int(ctx, 1));
	return 1;
}

static duk_ret_t count_args(duk_context *ctx)
{
	duk_push_int(ctx, duk_get_top(ctx));
	return 1;
}

static duk_ret_t thrower(duk_context *ctx)
{
	return duk_error(ctx, DUK_ERR_TYPE_ERROR, "bad thing %d", 7);
}

static duk_ret_t get_this_x(duk_context *ctx)
{
	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "x");
	return 1;
}

/* calls a throwing getter then carries on, the error must still surface */
static duk_ret_t swallow(duk_context *ctx)
{
	duk_push_global_object(ctx);
	duk_get_prop_string(ctx, -1, "boom");
	duk_pop_2(ctx);
	duk_push_int(ctx, 1);
	return 1;
}

static duk_ret_t safe_two(duk_context *ctx, void *udata)
{
	CHECK(duk_get_top(ctx) == 1);
	duk_push_int(ctx, 10);
	duk_push_int(ctx, 20);
	return 2;
}

static duk_ret_t safe_fail(duk_context *ctx, void *udata)
{
	return duk_error(ctx, DUK_ERR_RANGE_ERROR, "nope");
}

static duk_ret_t body_fn(duk_context *ctx, void *priv)
{
	CHECK(priv == &thing_priv);
	CHECK(duk_get_top(ctx) == 2);
	duk_concat(ctx, 2);
	return 1;
}

static JSValue native_method(JSContext *jctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return qjsduk_invoke(jctx, this_val, argc, argv, body_fn, &thing_priv);
}

static void test_stack(duk_context *ctx)
{
	duk_idx_t base = duk_get_top(ctx);
	CHECK(base == 0);
	duk_push_int(ctx, 1);
	duk_push_int(ctx, 2);
	duk_push_int(ctx, 3);
	CHECK(duk_get_top(ctx) == 3);
	duk_dup(ctx, 0);           /* 1 2 3 1 */
	CHECK(duk_get_int(ctx, -1) == 1);
	duk_insert(ctx, 1);        /* 1 1 2 3 */
	CHECK(duk_get_int(ctx, 1) == 1 && duk_get_int(ctx, 2) == 2 && duk_get_int(ctx, 3) == 3);
	duk_remove(ctx, 0);        /* 1 2 3 */
	CHECK(duk_get_top(ctx) == 3 && duk_get_int(ctx, 0) == 1);
	duk_push_int(ctx, 9);
	duk_replace(ctx, 0);       /* 9 2 3 */
	CHECK(duk_get_top(ctx) == 3 && duk_get_int(ctx, 0) == 9);
	duk_replace(ctx, -1);      /* 9 2 (replace top with itself is a pop) */
	CHECK(duk_get_top(ctx) == 2);
	duk_set_top(ctx, 4);
	CHECK(duk_get_top(ctx) == 4 && duk_is_undefined(ctx, 3));
	CHECK(duk_get_type(ctx, 10) == DUK_TYPE_NONE);
	CHECK(duk_normalize_index(ctx, -1) == 3);
	duk_pop_n(ctx, 4);
	CHECK(duk_get_top(ctx) == 0);
}

static void test_values(duk_context *ctx)
{
	size_t len;
	duk_push_string(ctx, "hello");
	CHECK(duk_is_string(ctx, -1));
	CHECK(strcmp(duk_get_string(ctx, -1), "hello") == 0);
	duk_push_number(ctx, 3.5);
	CHECK(strcmp(duk_to_string(ctx, -1), "3.5") == 0);
	CHECK(duk_is_string(ctx, -1));
	duk_push_number(ctx, 1e20);
	CHECK(duk_to_int(ctx, -1) == INT32_MAX);
	duk_push_number(ctx, -5);
	CHECK(duk_to_uint(ctx, -1) == 0);
	duk_push_string(ctx, "");
	CHECK(duk_to_boolean(ctx, -1) == 0 && duk_is_boolean(ctx, -1));
	duk_push_lstring(ctx, "a\0b", 3);
	duk_to_lstring(ctx, -1, &len);
	CHECK(len == 3);
	duk_push_null(ctx);
	CHECK(duk_is_null(ctx, -1) && duk_get_type(ctx, -1) == DUK_TYPE_NULL);
	duk_push_sprintf(ctx, "%d-%s", 5, "x");
	CHECK(strcmp(duk_get_string(ctx, -1), "5-x") == 0);
	duk_push_string(ctx, "a");
	duk_push_int(ctx, 1);
	duk_push_boolean(ctx, 1);
	duk_concat(ctx, 3);
	CHECK(strcmp(duk_get_string(ctx, -1), "a1true") == 0);
	CHECK(duk_opt_boolean(ctx, 99, 1) == 1);
	duk_set_top(ctx, 0);
}

static void test_props(duk_context *ctx, JSContext *jctx)
{
	duk_push_object(ctx);
	duk_push_int(ctx, 5);
	duk_put_prop_string(ctx, -2, "five");
	duk_push_string(ctx, "secret");
	duk_put_prop_string(ctx, -2, HIDDEN);
	duk_push_string(ctx, "k");
	duk_push_int(ctx, 6);
	duk_put_prop(ctx, -3);
	duk_push_int(ctx, 7);
	duk_put_prop_index(ctx, -2, 3);
	CHECK(duk_get_prop_string(ctx, -1, "five") && duk_get_int(ctx, -1) == 5);
	duk_pop(ctx);
	CHECK(duk_get_prop_string(ctx, -1, HIDDEN));
	CHECK(strcmp(duk_get_string(ctx, -1), "secret") == 0);
	duk_pop(ctx);
	/* hidden name pushed as a value works as a key */
	duk_push_string(ctx, HIDDEN);
	CHECK(duk_is_string(ctx, -1));
	CHECK(duk_get_prop(ctx, -2));
	CHECK(strcmp(duk_get_string(ctx, -1), "secret") == 0);
	duk_pop(ctx);
	duk_push_string(ctx, HIDDEN);
	CHECK(duk_has_prop(ctx, -2));
	CHECK(!duk_get_prop_string(ctx, -1, "missing"));
	duk_pop(ctx);
	CHECK(duk_get_prop_index(ctx, -1, 3) && duk_get_int(ctx, -1) == 7);
	duk_pop(ctx);
	CHECK(duk_del_prop_string(ctx, -1, "five"));
	CHECK(!duk_get_prop_string(ctx, -1, "five"));
	duk_pop(ctx);
	duk_put_global_string(ctx, "obj");
	CHECK(duk_get_top(ctx) == 0);
	/* the hidden property is invisible to scripts */
	CHECK(strcmp(eval_str(jctx, "JSON.stringify(Reflect.ownKeys(obj).map(String))"), "[\"3\",\"k\"]") == 0);
	CHECK(duk_get_global_string(ctx, "obj"));
	duk_pop(ctx);
	/* pointers as keys */
	duk_push_object(ctx);
	duk_push_pointer(ctx, &thing_priv);
	duk_push_int(ctx, 11);
	duk_put_prop(ctx, -3);
	duk_push_pointer(ctx, &thing_priv);
	CHECK(duk_get_prop(ctx, -2) && duk_get_int(ctx, -1) == 11);
	duk_pop(ctx);
	duk_push_pointer(ctx, &thing_priv);
	CHECK(duk_get_type(ctx, -1) == DUK_TYPE_POINTER);
	CHECK(duk_get_pointer(ctx, -1) == &thing_priv);
	duk_set_top(ctx, 0);
	/* prototypes */
	duk_push_object(ctx);
	duk_push_object(ctx);
	duk_push_int(ctx, 1);
	duk_put_prop_string(ctx, -2, "inherited");
	duk_set_prototype(ctx, -2);
	CHECK(duk_get_prop_string(ctx, -1, "inherited"));
	duk_pop(ctx);
	duk_get_prototype(ctx, -1);
	CHECK(duk_get_type(ctx, -1) == DUK_TYPE_OBJECT);
	duk_set_top(ctx, 0);
	/* def_prop getter */
	duk_push_global_object(ctx);
	duk_push_string(ctx, "gx");
	duk_push_c_function(ctx, count_args, 0);
	duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_ENUMERABLE | DUK_DEFPROP_HAVE_CONFIGURABLE | DUK_DEFPROP_CONFIGURABLE);
	CHECK(duk_get_top(ctx) == 1);
	duk_pop(ctx);
	CHECK(strcmp(eval_str(jctx, "gx"), "0") == 0);
}

static void test_functions(duk_context *ctx, JSContext *jctx)
{
	duk_push_c_function(ctx, add2, 2);
	duk_put_global_string(ctx, "add2");
	duk_push_c_function(ctx, count_args, DUK_VARARGS);
	duk_put_global_string(ctx, "nargs");
	duk_push_c_function(ctx, thrower, 0);
	duk_put_global_string(ctx, "thrower");
	duk_push_c_function(ctx, get_this_x, 0);
	duk_put_global_string(ctx, "getx");
	duk_push_c_function(ctx, swallow, 0);
	duk_put_global_string(ctx, "swallow");

	CHECK(strcmp(eval_str(jctx, "add2(2, 3)"), "5") == 0);
	CHECK(strcmp(eval_str(jctx, "add2(2)"), "2") == 0);
	CHECK(strcmp(eval_str(jctx, "add2(1, 2, 3)"), "3") == 0);
	CHECK(strcmp(eval_str(jctx, "nargs(1, 2, 3, 4)"), "4") == 0);
	CHECK(strcmp(eval_str(jctx, "try { thrower() } catch (e) { (e instanceof TypeError) + e.message }"), "truebad thing 7") == 0);
	CHECK(strcmp(eval_str(jctx, "({x: 'me', getx}).getx()"), "me") == 0);
	CHECK(strcmp(eval_str(jctx, "Object.defineProperty(globalThis, 'boom', {get() { throw new Error('kaboom') }}); try { swallow(); 'no' } catch (e) { e.message }"), "kaboom") == 0);
	/* nested: C -> JS -> C */
	CHECK(strcmp(eval_str(jctx, "[1,2,3].map(v => add2(v, v)).join()"), "2,4,6") == 0);
	CHECK(duk_get_top(ctx) == 0);

	/* calling JS from C */
	CHECK(duk_get_global_string(ctx, "add2"));
	duk_push_int(ctx, 40);
	duk_push_int(ctx, 2);
	CHECK(duk_pcall(ctx, 2) == DUK_EXEC_SUCCESS);
	CHECK(duk_get_int(ctx, -1) == 42);
	duk_pop(ctx);
	JS_FreeValue(jctx, eval(jctx, "function fail(a) { throw new RangeError('r' + a) }"));
	duk_get_global_string(ctx, "fail");
	duk_push_int(ctx, 1);
	CHECK(duk_pcall(ctx, 1) == DUK_EXEC_ERROR);
	CHECK(strcmp(duk_safe_to_string(ctx, -1), "RangeError: r1") == 0);
	duk_pop(ctx);
	duk_get_global_string(ctx, "fail");
	duk_push_int(ctx, 2);
	CHECK(duk_pcall(ctx, 1) == DUK_EXEC_ERROR);
	CHECK(strstr(duk_safe_to_stacktrace(ctx, -1), "at fail") != NULL);
	duk_pop(ctx);
	/* pcall_method passes this */
	JS_FreeValue(jctx, eval(jctx, "function me() { return this.v }"));
	duk_get_global_string(ctx, "me");
	duk_push_object(ctx);
	duk_push_string(ctx, "thisval");
	duk_put_prop_string(ctx, -2, "v");
	CHECK(duk_pcall_method(ctx, 0) == DUK_EXEC_SUCCESS);
	CHECK(strcmp(duk_get_string(ctx, -1), "thisval") == 0);
	duk_pop(ctx);
	CHECK(duk_get_top(ctx) == 0);

	/* safe_call */
	duk_push_int(ctx, 1);
	CHECK(duk_safe_call(ctx, safe_two, NULL, 1, 1) == DUK_EXEC_SUCCESS);
	CHECK(duk_get_top(ctx) == 1 && duk_get_int(ctx, 0) == 10);
	duk_pop(ctx);
	CHECK(duk_safe_call(ctx, safe_fail, NULL, 0, 2) == DUK_EXEC_ERROR);
	CHECK(duk_get_top(ctx) == 2);
	CHECK(strcmp(duk_safe_to_string(ctx, 0), "RangeError: nope") == 0);
	duk_set_top(ctx, 0);

	/* compile a handler function */
	duk_push_string(ctx, "function (event) { return event + 1 } // trailing");
	duk_push_string(ctx, "handler");
	CHECK(duk_pcompile(ctx, DUK_COMPILE_FUNCTION) == DUK_EXEC_SUCCESS);
	duk_push_int(ctx, 41);
	CHECK(duk_pcall(ctx, 1) == DUK_EXEC_SUCCESS && duk_get_int(ctx, -1) == 42);
	duk_pop(ctx);
	duk_push_string(ctx, "function (event) { return ( }");
	duk_push_string(ctx, "bad");
	CHECK(duk_pcompile(ctx, DUK_COMPILE_FUNCTION) == DUK_EXEC_ERROR);
	duk_pop(ctx);

	/* duk_call error becomes pending, returned from a body */
	CHECK(duk_get_top(ctx) == 0);
}

static void test_invoke(duk_context *ctx, JSContext *jctx)
{
	JSValue obj = JS_NewObjectClass(jctx, thing_class);
	JS_SetOpaque(obj, &thing_priv);
	JS_SetPropertyStr(jctx, obj, "m", JS_NewCFunction(jctx, native_method, "m", 2));
	JSValue g = JS_GetGlobalObject(jctx);
	JS_SetPropertyStr(jctx, g, "thing", obj);
	JS_FreeValue(jctx, g);
	CHECK(strcmp(eval_str(jctx, "thing.m('ab', 'cd')"), "abcd") == 0);

	/* private lookup through the magic property */
	duk_get_global_string(ctx, "thing");
	CHECK(duk_get_prop_string(ctx, -1, PRIVATE));
	CHECK(duk_get_pointer(ctx, -1) == &thing_priv);
	duk_pop(ctx);
	duk_push_object(ctx);
	CHECK(!duk_get_prop_string(ctx, -1, PRIVATE));
	CHECK(duk_get_pointer(ctx, -1) == NULL);
	duk_set_top(ctx, 0);

	/* buffers */
	uint8_t *data = duk_push_buffer(ctx, 16, 0);
	CHECK(data != NULL && data[5] == 0);
	data[5] = 200;
	duk_push_buffer_object(ctx, -1, 0, 16, DUK_BUFOBJ_UINT8CLAMPEDARRAY);
	duk_put_global_string(ctx, "clamped");
	duk_pop(ctx);
	CHECK(strcmp(eval_str(jctx, "clamped[5] + ':' + clamped.length + ':' + (clamped[0] = 300, clamped[0])"), "200:16:255") == 0);

	/* errors */
	duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "made %s", "here");
	CHECK(strcmp(duk_safe_to_string(ctx, -1), "TypeError: made here") == 0);
	duk_pop(ctx);
	CHECK(duk_get_top(ctx) == 0);
}

int main(void)
{
	JSRuntime *rt = JS_NewRuntime();
	JSContext *jctx = JS_NewContext(rt);
	JSClassDef def = { "Thing" };
	duk_context *ctx;
	JSValue err;

	JS_NewClassID(rt, &thing_class);
	JS_NewClass(rt, thing_class, &def);

	ctx = qjsduk_create(jctx, NULL);
	CHECK(qjsduk_from_js(jctx) == ctx);

	test_stack(ctx);
	test_values(ctx);
	test_props(ctx, jctx);
	test_functions(ctx, jctx);
	test_invoke(ctx, jctx);

	CHECK(!qjsduk_take_error(ctx, &err));

	qjsduk_destroy(ctx);
	JS_FreeContext(jctx);
	JS_FreeRuntime(rt);

	printf("%d checks, %d failures\n", checks, failures);
	return failures != 0;
}
