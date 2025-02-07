/**
 * @file UserListDialog.h
 * @brief
 * @author Lyubomir Filipov
 * @date Apr 16, 2022
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

#pragma once

#include <set>
#include "FilterListDialog.h"

namespace Mattermost {

class UserListDialogEntry {
	BackendUser* 	user;
	bool			isAvailableForChoose;
};

struct UserListDialogConfig {
	QString title;
	QString description;
};

class UserListDialog: public FilterListDialog {
public:
	UserListDialog (const UserListDialogConfig& cfg, const std::map<QString, BackendUser>& allUsers, const QSet<const BackendUser*>* alreadyExistingUsers, QWidget *parent);
	UserListDialog (const UserListDialogConfig& cfg, const std::vector<const BackendUser*>& allUsers, const QSet<const BackendUser*>* alreadyExistingUsers, QWidget *parent);
	virtual ~UserListDialog ();
public:
    const BackendUser* getSelectedUser ();
    void showContextMenu (const QPoint& pos)	override;
private:

    struct NameComparator {
    	bool operator () (const BackendUser* const& lhs, const BackendUser* const& rhs);
    };
    void create (const std::set<const BackendUser*, NameComparator>& users, const QSet<const BackendUser*>* alreadyExistingUsers);

};

} /* namespace Mattermost */
