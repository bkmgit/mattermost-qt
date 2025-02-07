/**
 * @file OutgoingPostCreator.cpp
 * @brief
 * @author Lyubomir Filipov
 * @date Feb 20, 2022
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

#include "OutgoingPostCreator.h"

#include <QDragMoveEvent>
#include <QMimeData>
#include <QDebug>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include "backend/Backend.h"
#include "chat-area/PostsListWidget.h"
#include "OutgoingPostPanel.h"
#include "NewPollDialog.h"
#include "OutgoingAttachmentList.h"
#include "choose-emoji-dialog/ChooseEmojiDialogWrapper.h"

#include "ui_OutgoingPostCreator.h"

namespace Mattermost {

struct OutgoingPostData {
	const BackendPost*					postToEdit;			//!< Post to be edited. If nullptr - start a new post
	std::unique_ptr<BackendNewPollData> pollData;			//!< Poll data, used when creating a poll
	QString 							message;			//!< Message text
	QList<QString>						attachmentPaths;	//!< List of file paths waiting to be attached
	QList<QString>						attachmentIds;		//!< List of file paths already uploaded to the Mattermost server
};

OutgoingPostCreator::OutgoingPostCreator(QWidget *parent)
:QWidget(parent)
,ui(new Ui::OutgoingPostCreator)
,postToEdit (nullptr)
,attachmentList (nullptr)
,isConnected (true)
{
    ui->setupUi(this);
}

void OutgoingPostCreator::init (Backend& backend, BackendChannel& channel, OutgoingPostPanel& panel, PostsListWidget& postsListWidget, QBoxLayout* attachmentParent)
{
	this->backend = &backend;
	this->channel = &channel;
	this->panel = &panel;

	this->attachmentParent = attachmentParent;
	/**
	 * the escape key erases currently entered text or cancels a message edit
	 */
	connect (ui->textEdit, &MessageTextEditWidget::escapePressed, [this] {
		ui->textEdit->clear();
		postToEdit = nullptr;
		postEditFinished ();
	});

	//initiate editing of last post, after an up arrow is pressed
	connect (ui->textEdit, &MessageTextEditWidget::upArrowPressed, [this, &postsListWidget] {
		QListWidgetItem* post = postsListWidget.getLastOwnPost ();

		if (post) {
			postsListWidget.initiatePostEdit (*post);
		}
	});

	//expand the text edit box so that it's entire text will be visible
	connect (ui->textEdit, &MessageTextEditWidget::textChanged, [this] {
		updateSendButtonState ();

		int height = ui->textEdit->document()->size().toSize().height();

		if (height > ui->textEdit->maximumHeight()) {
			height = ui->textEdit->maximumHeight();
		}

		emit heightChanged (height);
	});

	/*
	 * Send new post after pressing enter or clicking the 'Send' button
	 */
	connect (&panel.sendButton(), &QPushButton::clicked, this, &OutgoingPostCreator::sendPostButtonAction);
	connect (ui->textEdit, &MessageTextEditWidget::enterPressed, this, &OutgoingPostCreator::sendPostButtonAction);

	connect (&panel.attachButton(), &QPushButton::clicked, this, &OutgoingPostCreator::onAttachButtonClick);

	connect (&panel.addEmojiButton(), &QPushButton::clicked, [this] {
		showEmojiDialog ([this] (Emoji emoji){

			auto* textEdit = ui->textEdit;
			textEdit->insertPlainText (" :" + emoji.name + ": ");
			textEdit->setFocus ();
		});
	});

	updateSendButtonState();

	connectLambda (&backend, &Backend::onWebSocketConnect, [this] {
		isConnected = true;
		updateSendButtonState();
	});

	connectLambda (&backend, &Backend::onWebSocketDisconnect, [this] {
		isConnected = false;
		updateSendButtonState();
	});

	connect (&sendRetryTimer, &QTimer::timeout, [this] {
		qDebug () << "Post send retry";
		prepareAndSendPost ();
	});
}

OutgoingPostCreator::~OutgoingPostCreator()
{
	for (auto& it: signalConnections) {
		disconnect (it);
	}

	delete ui;
}

void OutgoingPostCreator::setStatusLabelText (const QString& string)
{
	panel->label().setText (string);
}

void OutgoingPostCreator::onAttachButtonClick ()
{
	QStringList files = QFileDialog::getOpenFileNames (this, "Select File(s) to attach");

	if (files.empty()) {
		return;
	}

	createAttachmentList (files);
}

void OutgoingPostCreator::postEditInitiated (BackendPost& post)
{
	if (isCreatingPost()) {
		qDebug () << "Post edit requested while creating post";
		emit postEditFinished();
		return;
	}

	ui->textEdit->setText (post.message);
	ui->textEdit->setFocus ();
	ui->textEdit->moveCursor (QTextCursor::End);
	postToEdit = &post;
}

static NewPollDialog* newPollDialog;

void OutgoingPostCreator::sendPostButtonAction ()
{
	QString message = ui->textEdit->toPlainText ();

	//do not send empty messages
	if (message.isEmpty() && !attachmentList) {
		return;
	}

	if (message == "/poll") {
		newPollDialog = new NewPollDialog (this);

		connect (newPollDialog, &QDialog::accepted, [this] {

			outgoingPostData = std::make_unique<OutgoingPostData>();
			outgoingPostData->pollData = std::make_unique<BackendNewPollData> (newPollDialog->getData ());

			sendRetryTimer.start (10000);
			ui->textEdit->setReadOnly (true);
			updateSendButtonState ();
			prepareAndSendPost ();
			setStatusLabelText ("Sending message...");

		});

		newPollDialog->show();
		return;
	}

	outgoingPostData = std::make_unique<OutgoingPostData>();

	outgoingPostData->message = message;
	outgoingPostData->postToEdit = postToEdit;
	postToEdit = nullptr;


	if (attachmentList) {
		outgoingPostData->attachmentPaths = attachmentList->getAllFiles();
		attachmentList->setDisableInput (true);
	}

	sendRetryTimer.start (10000);
	ui->textEdit->setReadOnly (true);
	updateSendButtonState ();
	prepareAndSendPost ();
	setStatusLabelText ("Sending message...");
}

void OutgoingPostCreator::prepareAndSendPost ()
{
	if (outgoingPostData->attachmentPaths.isEmpty()) {
		sendPost ();
		return;
	}

	for (auto it = outgoingPostData->attachmentPaths.begin(); it != outgoingPostData->attachmentPaths.end(); ++it) {
		auto& file = *it;
		backend->uploadFile (*channel, file, [this, it] (QString fileId) {

			outgoingPostData->attachmentIds.push_back (fileId);
			outgoingPostData->attachmentPaths.erase (it);
			size_t uploadedFilesCount = outgoingPostData->attachmentIds.size();
			size_t remainingFileCount = outgoingPostData->attachmentPaths.size();

			setStatusLabelText ("Attached file " + QString::number (uploadedFilesCount)
					+ " of " + QString::number (uploadedFilesCount + remainingFileCount));

			qDebug () << "Remaining file count: " << remainingFileCount;

			if (remainingFileCount == 0) {
				sendPost ();
			}
		});
	}
}

void OutgoingPostCreator::sendPost ()
{
	QString attachmentsLogStr (outgoingPostData->attachmentIds.isEmpty() ? "" : " (+attachments)");

	if (outgoingPostData->postToEdit) {
		qDebug () << "Send post edit" << attachmentsLogStr;
		backend->editPost (outgoingPostData->postToEdit->id, outgoingPostData->message, outgoingPostData->attachmentIds);
	} else if (outgoingPostData->pollData) {
		backend->addPoll (*channel, *outgoingPostData->pollData);
	} else {
		qDebug () << "Send post" << attachmentsLogStr;
		backend->addPost (*channel, outgoingPostData->message, outgoingPostData->attachmentIds);
	}
}

void OutgoingPostCreator::onDragEnterEvent (QDragEnterEvent* event)
{
	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
	}
}

void OutgoingPostCreator::onDragMoveEvent (QDragMoveEvent* event)
{
	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
	}
}

void OutgoingPostCreator::onDropEvent (QDropEvent* event)
{
	if (isWaitingForPostServerResponse ()) {
		qDebug() << "Cannot attach files while sending a post";
		return;
	}

	QStringList files;

	for (auto& url: event->mimeData ()->urls()) {
		files.push_back (url.toLocalFile());
	}

	createAttachmentList (files);
}

void OutgoingPostCreator::onPostReceived (BackendPost& post)
{
	if (outgoingPostData && (post.isOwnPost() || post.isOwnPollPost())) {

		sendRetryTimer.stop ();

		setStatusLabelText ("");

		//reset the 'editing post' state
		if (outgoingPostData->postToEdit) {
			emit postEditFinished();
		}

		outgoingPostData.reset();

		if (attachmentList) {
			delete (attachmentList);
			attachmentList = nullptr;
		}

		ui->textEdit->clear();
		ui->textEdit->setReadOnly (false);
		updateSendButtonState ();
	}
}

void OutgoingPostCreator::createAttachmentList (QStringList& files)
{
	if (!attachmentList) {
		attachmentList = new OutgoingAttachmentList (this);

		attachmentParent->insertWidget (0, attachmentList);
		updateSendButtonState ();

		connect (attachmentList, &OutgoingAttachmentList::deleted, [this] {
			attachmentParent->removeWidget (attachmentList);
			delete (attachmentList);
			attachmentList = nullptr;
			updateSendButtonState ();
		});
	}

	for (auto& filename: files) {
		attachmentList->addFile (filename);
	}
}

void OutgoingPostCreator::updateSendButtonState ()
{
	bool sendButtonEnabled = true;
	QString tooltipText;

	if (!isConnected) { //no connection
		tooltipText = "Server connection lost";

		if (outgoingPostData) {
			tooltipText += ", sending message";
		}

		sendButtonEnabled = false;
	} else if (outgoingPostData) { //sending message in progress
		sendButtonEnabled = false;
		tooltipText = "Waiting for server response";
	}

	bool attachButtonEnabled = sendButtonEnabled;

	panel->attachButton().setDisabled (!attachButtonEnabled);
	panel->attachButton().setToolTip (tooltipText);

	/**
	 * Send button is disabled if the post text area is empty
	 */
	if (sendButtonEnabled && !isCreatingPost()) {
		sendButtonEnabled = false;
		tooltipText = "Cannot send empty message";
	}

	panel->sendButton().setDisabled (!sendButtonEnabled);
	panel->sendButton().setToolTip (tooltipText);
}

/**
 * returns true if the client is waiting for server response after sending or editing a post,
 * or if the user is in the process of creating or editing a post (if there is typed text or attached files)
 */
bool OutgoingPostCreator::isCreatingPost ()
{
	if (isWaitingForPostServerResponse ()) {
		return true;
	}

	QString message = ui->textEdit->toPlainText ();

	if (message.isEmpty() && !attachmentList) {
		return false;
	}

	return true;
}

/**
 * returns true if the client is waiting for server response after sending or editing a post
 */
bool OutgoingPostCreator::isWaitingForPostServerResponse ()
{
	return outgoingPostData ? true : false;
}

} /* namespace Mattermost */
