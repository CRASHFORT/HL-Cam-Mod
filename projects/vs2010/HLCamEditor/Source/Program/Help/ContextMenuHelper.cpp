#include "PrecompiledHeader.hpp"
#include "ContextMenuHelper.hpp"

namespace App
{
	namespace Help
	{
		ContextMenuHelper::ContextMenuHelper() :
			CurrentValueOffset(10)
		{
			ContextMenu.CreatePopupMenu();
		}

		void ContextMenuHelper::AddEntry(const std::string& text, std::function<void()>&& callback, bool disabled)
		{
			ContextMenu.AppendMenuA(MF_BYPOSITION | MF_STRING | (disabled ? MF_GRAYED : 0), CurrentValueOffset, text.c_str());
			MenuEntries[CurrentValueOffset] = callback;
			CurrentValueOffset++;
		}

		void ContextMenuHelper::AddSeparator()
		{
			ContextMenu.AppendMenuA(MF_SEPARATOR | MF_BYPOSITION);
			CurrentValueOffset++;
		}

		int ContextMenuHelper::Open(CPoint pos, CWnd* menuparentwindow)
		{
			auto result = ContextMenu.TrackPopupMenu(TPM_RETURNCMD, pos.x, pos.y, menuparentwindow);

			const auto& targetfunc = MenuEntries[result];

			if (targetfunc)
			{
				targetfunc();
			}

			return result;
		}
	}
}
