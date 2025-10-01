#include "mouse.h"

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
	return Mouse::Initialize(env, exports);
}

NODE_API_MODULE(addon, InitAll)
