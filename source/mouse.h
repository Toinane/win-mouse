#ifndef _MOUSE_H
#define _MOUSE_H

#include <napi.h>

#include "mouse_hook.h"

struct MouseEvent {
	LONG x;
	LONG y;
	WPARAM type;
};

const unsigned int BUFFER_SIZE = 10;

class Mouse : public Napi::ObjectWrap<Mouse> {
	public:
		static Napi::Object Initialize(Napi::Env env, Napi::Object exports);
		static Napi::FunctionReference constructor;

		void Stop();
		void HandleEvent(WPARAM, POINT);
		void HandleSend();

		explicit Mouse(const Napi::CallbackInfo& info);
		~Mouse();

		static Napi::Value New(const Napi::CallbackInfo& info);
		Napi::Value Destroy(const Napi::CallbackInfo& info);
		Napi::Value AddRef(const Napi::CallbackInfo& info);
		Napi::Value RemoveRef(const Napi::CallbackInfo& info);

	private:
		MouseHookRef hook_ref;
		MouseEvent* eventBuffer[BUFFER_SIZE];
		unsigned int readIndex;
		unsigned int writeIndex;
		Napi::FunctionReference event_callback;
		uv_async_t* async;
		uv_mutex_t lock;
		volatile bool stopped;
};

#endif
