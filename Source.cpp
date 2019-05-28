#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_dx9.h>
#include <ImGui/imgui_impl_win32.h>
#include <ImGui/imgui_stl.h>
#include <d3d9.h>
#include <string>
#include <map>
#include <process_utils.h>
#include <tchar.h>
#include <psapi.h>
#include <filesystem>
#include <vector>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
static LPDIRECT3DDEVICE9 d3d_device = NULL;
static D3DPRESENT_PARAMETERS d3d_present_parameters;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg) {
		case WM_SIZE:
			if (d3d_device != NULL && wParam != SIZE_MINIMIZED) {
				ImGui_ImplDX9_InvalidateDeviceObjects();
				d3d_present_parameters.BackBufferWidth = LOWORD(lParam);
				d3d_present_parameters.BackBufferHeight = HIWORD(lParam);
				HRESULT hr = d3d_device->Reset(&d3d_present_parameters);
				if (hr == D3DERR_INVALIDCALL)
					IM_ASSERT(0);
				ImGui_ImplDX9_CreateDeviceObjects();
			}
			return 0;
		case WM_SYSCOMMAND:
			if ((wParam & 0xfff0) == SC_KEYMENU)
				return 0;
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

int main(int argc, char* argv[]) {
	WNDCLASSEX window_class = {
		sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "kaninject",
		NULL
	};
	RegisterClassEx(&window_class);
	int screen_size_x = GetSystemMetrics(SM_CXSCREEN), screen_size_y = GetSystemMetrics(SM_CYSCREEN);
	HWND window =		CreateWindow("kaninject", "kaninject - dll injector", WS_OVERLAPPEDWINDOW, screen_size_x / 8,
 screen_size_y / 8,
		screen_size_x / 3, screen_size_y / 2,
		NULL, NULL, window_class.hInstance, NULL);

	auto d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (d3d == NULL) {
		UnregisterClass("kaninject", window_class.hInstance);
		return 0;
	}

	memset(&d3d_present_parameters, 0, sizeof(D3DPRESENT_PARAMETERS));
	d3d_present_parameters.Windowed = true;
	d3d_present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3d_present_parameters.BackBufferFormat = D3DFMT_UNKNOWN;
	d3d_present_parameters.AutoDepthStencilFormat = D3DFMT_D16;
	d3d_present_parameters.EnableAutoDepthStencil = true;
	d3d_present_parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING,
	                      &d3d_present_parameters, &d3d_device) < 0) {
		d3d->Release();
		UnregisterClass("kaninject", window_class.hInstance);
		return 0;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& imgui_io = ImGui::GetIO();
	//imgui_io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(d3d_device);

	MSG msg;
	memset(&msg, 0, sizeof(MSG));
	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		static char current_dll_path[128], current_process_name[32];
		static int process_id = 0;
		static bool open_process_list = false;

		static ImGuiInputTextCallback on_process_name_changed = [](ImGuiInputTextCallbackData* callback_data) -> int {
			if (!callback_data->UserData)
				return 0;
			int temp_process_id = *reinterpret_cast<int*>(callback_data->UserData);
			if (temp_process_id > 0 && temp_process_id < 100000)
				ImGui::TextColored(ImVec4(0, 1, 0, 1), "found process: %s with pid: %d", callback_data->Buf,
				                   temp_process_id);
			else
				ImGui::TextColored(ImVec4(1, 1, 0, 1), "looking for process: %s...", callback_data->Buf);
			return 0;
		};

		ImGui::Begin("kaninject", nullptr,
		             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground);
		{
			RECT window_size;
			GetWindowRect(window, &window_size);
			ImGui::SetWindowPos(ImVec2(10, 10));
			ImGui::SetWindowSize(ImVec2(window_size.right - window_size.left, window_size.bottom - window_size.top));
			if (process_id > 0)
				ImGui::TextColored(ImVec4(0, 1, 0, 1), "pid: %d", process_id);
			ImGui::InputText("dll path", current_dll_path, sizeof(current_dll_path));
			int temp_process_id = get_process_id(current_process_name);
			ImGui::InputText("process name", current_process_name, sizeof(current_process_name) / sizeof(char),
			                 ImGuiInputTextFlags_CallbackAlways, on_process_name_changed,
			                 reinterpret_cast<void*>(&temp_process_id));

			if (ImGui::Button("inject!")) {
				load_library_inject(current_dll_path, process_id);
			}

			ImGui::SetWindowFocus();
			auto window_position = ImGui::GetWindowPos();
			ImGui::SetWindowPos(ImVec2(10, window_position.y));
			static std::vector<std::string> process_names;
			static std::vector<int> process_ids;
			if (ImGui::Button("refresh processes")) {
				process_names.clear();
				process_ids.clear();
				process_names.push_back("nigaa");
				for (size_t i = 0; i < 100000; i++) {
					auto temp_value = get_process_name_by_id(i);
					if (temp_value == std::nullopt)
						continue;
					process_names.push_back(std::move(get_process_name_by_id(i).value()));
				}
			}

			static int process_name_index = 0;
			ImGui::ListBoxHeader("process list");
			static char* listed_processes;
			if (ImGui::ListBox("processes", &process_name_index, process_names)) {
				process_id = get_process_id(process_names[process_name_index].c_str());
			}
			ImGui::ListBoxFooter();
			static char cheat_dir[256] = {"C:/Hacks/"};
			ImGui::InputText("cheat directory", cheat_dir, sizeof(cheat_dir) / sizeof(char));
			std::vector<std::string> static cheats;
			if (ImGui::Button("refresh available cheats")) {
				cheats.clear();
				std::filesystem::path path(cheat_dir);
				for (auto i = std::filesystem::directory_iterator(path); i != std::filesystem::directory_iterator(); i++
				) {
					auto temp = i->path();
					if (temp.extension() == ".dll")
						cheats.push_back(std::move(temp.string()));
					else
						continue;
				}
			}
			static int selected_cheat_index = 0;
			ImGui::ListBoxHeader("cheat list");
			if (ImGui::ListBox("available cheats", &selected_cheat_index, cheats)) {
				strcpy_s(current_dll_path, cheats[selected_cheat_index].c_str());
			}
			ImGui::ListBoxFooter();
		}
		ImGui::End();


		ImGui::EndFrame();
		d3d_device->SetRenderState(D3DRS_ZENABLE, false);
		d3d_device->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
		d3d_device->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
		d3d_device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 0), 1.0f, 0);

		if (d3d_device->BeginScene() >= 0) {
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			d3d_device->EndScene();
		}
		HRESULT result = d3d_device->Present(NULL, NULL, NULL, NULL);

		// Handle loss of D3D9 device
		if (result == D3DERR_DEVICELOST && d3d_device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
			ImGui_ImplDX9_InvalidateDeviceObjects();
			d3d_device->Reset(&d3d_present_parameters);
			ImGui_ImplDX9_CreateDeviceObjects();
		}
	}

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	if (d3d_device)
		d3d_device->Release();

	if (d3d)
		d3d->Release();
	DestroyWindow(window);
	UnregisterClass("kaninject", window_class.hInstance);
	return 0;
}
