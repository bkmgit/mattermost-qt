/**
 * @file TeamItem.cpp
 * @brief
 * @author Lyubomir Filipov
 * @date Dec 27, 2021
 *
 * Copyright 2021, 2022 Lyubomir Filipov
 *
 * This file is part of Mattermost-QT.
 *
 * Mattermost-QT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mattermost-QT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Mattermost-QT. if not, see https://www.gnu.org/licenses/.
 */

#include "TeamItem.h"

#include <QMenu>
#include <QStackedWidget>
#include "channel-tree/ChannelItemWidget.h"
#include "channel-tree/ChannelItem.h"
#include "chat-area/ChatArea.h"
#include "backend/Backend.h"
#include "channel-tree/ChannelTree.h"

namespace Mattermost {

TeamItem::TeamItem (QTreeWidget& parent, Backend& backend, const QString& name, const QString& teamId)
:ChannelTreeItem (&parent, QStringList() << name)
,backend (backend)
,teamId (teamId)
{
	QFont font;
	font.setBold (true);
	font.setPixelSize (14);
	setFont (0, font);

	setExpanded (true);
}

TeamItem::~TeamItem () = default;

void TeamItem::addChannel (BackendChannel& channel, QWidget *parent, QStackedWidget* chatAreaParent)
{
	ChannelItemWidget* itemWidget = new ChannelItemWidget (parent);
	itemWidget->setLabel (channel.display_name);

	ChannelItem* item = createChannelItem (backend, itemWidget);
	insertChild (getChannelIndex (channel), item);

	ChatArea* chatArea = new ChatArea (backend, channel, item, (QWidget*)chatAreaParent);
	chatAreaParent->addWidget(chatArea);

	item->setData(0, Qt::UserRole, QVariant::fromValue (chatArea));

	treeWidget()->setItemWidget (item, 0, itemWidget);

	connect (&channel, &BackendChannel::onLeave, [this, &channel, item, chatAreaParent, chatArea] {
		qDebug() << "delete channel " << channel.name;
		ChannelTree* treeWidget = static_cast<ChannelTree*> (this->treeWidget());

		chatAreaParent->removeWidget (chatArea);
		delete (chatArea);
		removeChild (item);
		delete (item);
		treeWidget->removeChannelToItem (channel.id);
	});

	backend.retrieveChannelMembers (channel);
	ChannelTree* treeWidget = static_cast<ChannelTree*> (this->treeWidget());
	treeWidget->addChannelToItem (channel.id, item);
}

int TeamItem::getChannelIndex (const BackendChannel& channel)
{
	int i = 0;
	for (; i < childCount(); ++i) {

		//item->setData(0, Qt::UserRole, QVariant::fromValue (chatArea));

		ChatArea* area = child(i)->data (0, Qt::UserRole).value<ChatArea*>();

		if (channel.last_post_at > area->channel.last_post_at) {
			break;
		}
	}

	return i;
}

} /* namespace Mattermost */

