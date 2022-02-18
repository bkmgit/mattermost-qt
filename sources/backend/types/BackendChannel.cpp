/**
 * @file BackendChannel.cpp
 * @brief
 * @author Lyubomir Filipov
 * @date Dec 1, 2021
 */

#include <QJsonObject>
#include <QDebug>
#include <QJsonArray>
#include "BackendChannel.h"
#include "backend/Storage.h"

namespace Mattermost {

uint32_t BackendChannel::getChannelType (const QJsonObject& jsonObject)
{
	switch (jsonObject.value("type").toString()[0].unicode()) {
	case 'O':
		return publicChannel;
		break;
	case 'P':
		return privateChannel;
		break;
	case 'D':
		return directChannel;
		break;
	default:
		return unknown;
		break;
	}
}

void ChannelMissingPosts::addSequence (ChannelMissingPostsSequence&& seq)
{
	//do not add empty sequence
	if (seq.postsToAdd.empty()) {
		return;
	}

	postsToAdd.emplace_back (std::move (seq));
}

BackendChannel::BackendChannel (const Storage& storage, const QJsonObject& jsonObject)
:storage (storage)
{
	id = jsonObject.value("id").toString();
	create_at = jsonObject.value("create_at").toVariant().toULongLong();
	update_at = jsonObject.value("update_at").toVariant().toULongLong();
	delete_at = jsonObject.value("delete_at").toVariant().toULongLong();
	team_id = jsonObject.value("team_id").toString();
	display_name = jsonObject.value("display_name").toString();
	name = jsonObject.value("name").toString();
	header = jsonObject.value("header").toString();
	purpose = jsonObject.value("purpose").toString();

	type = getChannelType (jsonObject);

	last_post_at = jsonObject.value("last_post_at").toInt();
	total_msg_count = jsonObject.value("total_msg_count").toInt();
	extra_update_at = jsonObject.value("extra_update_at").toInt();
	creator_id = jsonObject.value("creator_id").toString();
	scheme_id = jsonObject.value("scheme_id").toVariant();
	props = jsonObject.value("props").toVariant();
	referenceCount = 1;
}

BackendChannel::~BackendChannel () = default;

BackendPost* BackendChannel::addPost (const QJsonObject& postObject)
{
	posts.emplace_back (postObject);

	BackendPost* newPost = &posts.back ();
	newPost->author = storage.getUserById (newPost->user_id);
	return newPost;
}

void Mattermost::BackendChannel::addPost (const QJsonObject& postObject, std::list<BackendPost>::iterator position, ChannelMissingPostsSequence& currentSequence)
{
	/*
	 * Add a post.
	 * And add added post to the list of new posts
	 */
	BackendPost* newPost = &*posts.emplace(position, postObject);
	newPost->author = storage.getUserById (newPost->user_id);
	qDebug() << newPost->create_at << " " << newPost->id << " " << newPost->getDisplayAuthorName() << " " << newPost->message;
	currentSequence.postsToAdd.emplace_front (newPost);
}

void BackendChannel::addPosts (const QJsonArray& orderArray, const QJsonObject& postsObject)
{
	ChannelMissingPosts allMissingPosts;

	ChannelMissingPostsSequence currentMissingPostsSeq;

	std::list<BackendPost>::reverse_iterator currentLocalPost = posts.rbegin();

	for (const auto& newPostEl: orderArray) {

		QString newPostId = newPostEl.toString();

		//end of local posts list. Save the current missing post sequence and add all missing posts
		if (currentLocalPost == posts.rend()) {
			addPost (postsObject.find (newPostId).value().toObject(), posts.begin (), currentMissingPostsSeq);
			++currentLocalPost;
			continue;
		}

		//post already exists. No need to be added. Save the current missing post sequence and continue
		if (currentLocalPost->id == newPostId) {
			++currentLocalPost;
			allMissingPosts.addSequence (std::move (currentMissingPostsSeq));
			continue;
		}
	}
}

} /* namespace Mattermost */

