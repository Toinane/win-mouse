#include "mouse.h"

const char* LEFT_DOWN = "left-down";
const char* LEFT_UP = "left-up";
const char* RIGHT_DOWN = "right-down";
const char* RIGHT_UP = "right-up";
const char* MOVE = "move";

bool IsMouseEvent(WPARAM type) {
	return type == WM_LBUTTONDOWN ||
		type == WM_LBUTTONUP ||
		type == WM_RBUTTONDOWN ||
		type == WM_RBUTTONUP ||
		type == WM_MOUSEMOVE;
}

void OnMouseEvent(WPARAM type, POINT point, void* data) {
	Mouse* mouse = (Mouse*) data;
	mouse->HandleEvent(type, point);
}

void OnSend(uv_async_t* async) {
	Mouse* mouse = (Mouse*) async->data;
	mouse->HandleSend();
}

void OnClose(uv_handle_t* handle) {
	uv_async_t* async = (uv_async_t*) handle;
	delete async;
}

Napi::FunctionReference Mouse::constructor;

Mouse::Mouse(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Mouse>(info) {
	Napi::Env env = info.Env();
	
	async = new uv_async_t;
	async->data = this;

	for (size_t i = 0; i < BUFFER_SIZE; i++) {
		eventBuffer[i] = new MouseEvent();
	}

	readIndex = 0;
	writeIndex = 0;

	event_callback = Napi::Persistent(info[0].As<Napi::Function>());
	stopped = false;

	uv_async_init(uv_default_loop(), async, OnSend);
	uv_mutex_init(&lock);

	hook_ref = MouseHookRegister(OnMouseEvent, this);
}

Mouse::~Mouse() {
	Stop();
	uv_mutex_destroy(&lock);

	for (size_t i = 0; i < BUFFER_SIZE; i++) {
		delete eventBuffer[i];
	}
}

Napi::Object Mouse::Initialize(Napi::Env env, Napi::Object exports) {
	Napi::Function func = DefineClass(env, "Mouse", {
		InstanceMethod("destroy", &Mouse::Destroy),
		InstanceMethod("ref", &Mouse::AddRef),
		InstanceMethod("unref", &Mouse::RemoveRef)
	});

	constructor = Napi::Persistent(func);
	constructor.SuppressDestruct();

	exports.Set("Mouse", func);
	return exports;
}

void Mouse::Stop() {
	uv_mutex_lock(&lock);

	if (!stopped) {
		stopped = true;
		MouseHookUnregister(hook_ref);
		uv_close((uv_handle_t*) async, OnClose);
	}

	uv_mutex_unlock(&lock);
}

void Mouse::HandleEvent(WPARAM type, POINT point) {
	if(!IsMouseEvent(type) || stopped) return;

	uv_mutex_lock(&lock);

	if (!stopped) {
		eventBuffer[writeIndex]->x = point.x;
		eventBuffer[writeIndex]->y = point.y;
		eventBuffer[writeIndex]->type = type;
		writeIndex = (writeIndex + 1) % BUFFER_SIZE;
		uv_async_send(async);
	}

	uv_mutex_unlock(&lock);
}

void Mouse::HandleSend() {
	uv_mutex_lock(&lock);

	while (readIndex != writeIndex && !stopped) {
		MouseEvent e = {
			eventBuffer[readIndex]->x,
			eventBuffer[readIndex]->y,
			eventBuffer[readIndex]->type
		};
		readIndex = (readIndex + 1) % BUFFER_SIZE;
		const char* name;

		if (e.type == WM_LBUTTONDOWN) name = LEFT_DOWN;
		if (e.type == WM_LBUTTONUP) name = LEFT_UP;
		if (e.type == WM_RBUTTONDOWN) name = RIGHT_DOWN;
		if (e.type == WM_RBUTTONUP) name = RIGHT_UP;
		if (e.type == WM_MOUSEMOVE) name = MOVE;

		// Make sure we have a proper environment and handle scope
		if (!event_callback.IsEmpty()) {
			Napi::Env env = event_callback.Env();
			Napi::HandleScope scope(env);
			
			event_callback.Call({
				Napi::String::New(env, name),
				Napi::Number::New(env, e.x),
				Napi::Number::New(env, e.y)
			});
		}
	}

	uv_mutex_unlock(&lock);
}

Napi::Value Mouse::New(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	
	if (info.IsConstructCall()) {
		Mouse* mouse = new Mouse(info);
		return info.This();
	} else {
		std::vector<napi_value> ctor_args;
		ctor_args.push_back(info[0]);
		return constructor.New(ctor_args);
	}
}

Napi::Value Mouse::Destroy(const Napi::CallbackInfo& info) {
	this->Stop();
	return info.Env().Undefined();
}

Napi::Value Mouse::AddRef(const Napi::CallbackInfo& info) {
	uv_ref((uv_handle_t*) this->async);
	return info.Env().Undefined();
}

Napi::Value Mouse::RemoveRef(const Napi::CallbackInfo& info) {
	uv_unref((uv_handle_t*) this->async);
	return info.Env().Undefined();
}
