/**
 * @file BackendUser.cpp
 * @brief
 * @author Lyubomir Filipov
 * @date Nov 28, 2021
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

#include "BackendUser.h"

#include <QJsonObject>
#include <QVariant>

namespace Mattermost {

BackendUser::BackendUser ()
:allow_marketing (false)
,isLoginUser (false)
{
}

BackendUser::BackendUser (const QJsonObject& jsonObject)
{
	id = jsonObject.value("id").toString();
	create_at = jsonObject.value("create_at").toVariant().toULongLong();
	update_at = jsonObject.value("update_at").toVariant().toULongLong();
	delete_at = jsonObject.value("delete_at").toVariant().toULongLong();
	username = jsonObject.value("username").toString();
	auth_data = jsonObject.value("auth_data").toString();
	auth_service = jsonObject.value("auth_service").toString();
	email = jsonObject.value("email").toString();
	nickname = jsonObject.value("nickname").toString();
	first_name = jsonObject.value("first_name").toString();
	last_name = jsonObject.value("last_name").toString();
	position = jsonObject.value("position").toString();
	roles = jsonObject.value("roles").toString().split(',');
	allow_marketing = jsonObject.value("allow_marketing").toBool();
	notify_preps.deserialize (jsonObject.value("notify_props").toObject());
	last_password_update = jsonObject.value("last_password_update").toVariant().toULongLong();
	locale = jsonObject.value("locale").toString();
	timezone.deserialize (jsonObject.value("timezone").toObject());
	isLoginUser = false;
}

BackendUser::~BackendUser () = default;

QString BackendUser::getDisplayName () const
{
	if (!first_name.isEmpty()) {
		if (last_name.isEmpty()) {
			return first_name;
		}

		return first_name + " " + last_name;
	}

	/*
	 * For some users, first name and last name are empty. Use the username in this case
	 */
	return username;
}


} /* namespace Mattermost */

