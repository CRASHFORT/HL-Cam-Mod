#pragma once
#include <functional>
#include <unordered_map>

namespace App
{
	namespace Help
	{
		class ContextMenuHelper final
		{
		public:
			ContextMenuHelper();
			void AddEntry(const std::string& text, std::function<void()>&& callback, bool disabled = false);
			void AddSeparator();

			int Open(CPoint pos, CWnd* menuparentwindow);

		private:
			CMenu ContextMenu;
			int CurrentValueOffset;
			std::unordered_map<int, std::function<void()>> MenuEntries;
		};
	}
}
