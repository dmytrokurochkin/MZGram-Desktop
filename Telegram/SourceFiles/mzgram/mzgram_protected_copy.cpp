/*
This file is part of MZGram,
a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mzgram/mzgram_protected_copy.h"

#include "api/api_common.h"
#include "apiwrap.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_types.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "mzgram/mzgram_protected_content.h"
#include "storage/localimageloader.h"
#include "ui/chat/attach/attach_prepare.h"
#include "ui/image/image.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFile>

namespace MZGram {
namespace {

// What the server enforces, whatever the switch says: the message's own
// flag, or its chat's or user's "Restrict saving content".
[[nodiscard]] bool ProtectedOnServer(not_null<HistoryItem*> item) {
	return item->forbidsForwardOnServer()
		|| !item->history()->peer->allowsForwardingOnServer();
}

struct Copy {
	explicit Copy(const Api::SendAction &action) : action(action) {
	}

	Api::SendAction action;
	CopyPlan plan;
	TextWithTags text;
	Data::FileOrigin origin;
	PhotoData *photo = nullptr;
	DocumentData *document = nullptr;
	std::shared_ptr<Data::PhotoMedia> photoView;
	std::shared_ptr<Data::DocumentMedia> documentView;
};

[[nodiscard]] CopyMedia MediaOf(
		not_null<HistoryItem*> item,
		not_null<Copy*> copy) {
	const auto media = item->media();
	if (!media || media->webpage()) {
		return CopyMedia::None;
	} else if (media->invoice() || media->poll() || media->storyId()) {
		return CopyMedia::Other;
	} else if (const auto photo = media->photo()) {
		copy->photo = photo;
		copy->photoView = photo->createMediaView();
		return CopyMedia::Photo;
	} else if (const auto document = media->document()) {
		copy->document = document;
		copy->documentView = document->createMediaView();
		return document->isVoiceMessage()
			? CopyMedia::Voice
			: document->isVideoMessage()
			? CopyMedia::Round
			: CopyMedia::File;
	}
	return CopyMedia::Other;
}

// The photo as it was downloaded, or encoded again when only the image is
// kept in memory.
[[nodiscard]] QByteArray PhotoBytes(const Copy &copy) {
	const auto size = Data::PhotoSize::Large;
	auto result = copy.photoView->imageBytes(size);
	if (result.isEmpty()) {
		if (const auto image = copy.photoView->image(size)) {
			auto buffer = QBuffer(&result);
			image->original().save(&buffer, "JPG", 87);
		}
	}
	return result;
}

[[nodiscard]] bool PhotoOnDisk(const Copy &copy) {
	return copy.photoView->loaded() && !PhotoBytes(copy).isEmpty();
}

[[nodiscard]] bool DocumentOnDisk(const Copy &copy) {
	return !copy.document->filepath(true).isEmpty();
}

[[nodiscard]] bool FileOnDisk(const Copy &copy) {
	return copy.photo ? PhotoOnDisk(copy) : DocumentOnDisk(copy);
}

[[nodiscard]] bool DownloadFailed(const Copy &copy) {
	return copy.photo
		? copy.photo->failed(Data::PhotoSize::Large)
		: ((copy.document->status == FileDownloadFailed)
			|| copy.document->cancelled());
}

// Where a document goes when it is downloaded or held only in memory, so
// it can be uploaded again under its own name.
[[nodiscard]] QString CopyPath(not_null<DocumentData*> document) {
	const auto folder = QDir::tempPath()
		+ u"/MZGram/"_q
		+ QString::number(document->id);
	QDir().mkpath(folder);
	const auto name = document->filename();
	return folder + '/' + (name.isEmpty() ? u"file"_q : name);
}

[[nodiscard]] QByteArray ReadDocument(const Copy &copy) {
	auto file = QFile(copy.document->filepath(true));
	return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void SendText(not_null<ApiWrap*> api, const Copy &copy) {
	auto message = Api::MessageToSend(copy.action);
	message.textWithTags = copy.text;
	api->sendMessage(std::move(message));
}

void SendFile(not_null<ApiWrap*> api, const Copy &copy) {
	const auto caption = copy.plan.caption ? copy.text : TextWithTags();
	switch (copy.plan.kind) {
	case CopyKind::Photo: {
		auto file = Ui::PreparedFile(QString());
		file.content = PhotoBytes(copy);
		file.type = Ui::PreparedFile::Type::Photo;
		file.caption = caption;
		auto list = Ui::PreparedList();
		list.files.push_back(std::move(file));
		api->sendFiles(
			std::move(list),
			SendMediaType::Photo,
			nullptr,
			copy.action);
	} break;
	case CopyKind::Voice:
	case CopyKind::Round: {
		const auto voice = copy.document->voice();
		api->sendVoiceMessage(
			ReadDocument(copy),
			voice ? voice->waveform : VoiceWaveform(),
			copy.document->duration(),
			(copy.plan.kind == CopyKind::Round),
			copy.action);
	} break;
	case CopyKind::File: {
		auto file = Ui::PreparedFile(copy.document->filepath(true));
		file.displayName = copy.document->filename();
		file.caption = caption;
		auto list = Ui::PreparedList();
		list.files.push_back(std::move(file));
		api->sendFiles(
			std::move(list),
			SendMediaType::File,
			nullptr,
			copy.action);
	} break;
	default: break;
	}
}

void DownloadThenSend(
		not_null<ApiWrap*> api,
		std::shared_ptr<Copy> copy) {
	const auto session = &copy->action.history->session();
	if (copy->photo) {
		copy->photo->load(Data::PhotoSize::Large, copy->origin);
	} else {
		copy->document->save(copy->origin, CopyPath(copy->document));
	}
	if (FileOnDisk(*copy)) {
		SendFile(api, *copy);
		return;
	}
	session->downloaderTaskFinished(
	) | rpl::filter([=] {
		return FileOnDisk(*copy) || DownloadFailed(*copy);
	}) | rpl::take(1) | rpl::on_next([=] {
		if (FileOnDisk(*copy)) {
			SendFile(api, *copy);
		} else {
			LOG(("MZGram: protected file could not be downloaded, "
				"not copied."));
		}
	}, session->lifetime());
}

void SendCopy(
		not_null<ApiWrap*> api,
		not_null<HistoryItem*> item,
		const Api::SendAction &action,
		bool dropCaption) {
	auto copy = std::make_shared<Copy>(action);
	const auto media = MediaOf(item, copy.get());
	const auto &original = item->originalText();
	copy->text = TextWithTags{
		original.text,
		TextUtilities::ConvertEntitiesToTextTags(original.entities),
	};
	copy->origin = item->fullId();
	copy->plan = PlanCopy({
		.media = media,
		.hasText = !original.text.isEmpty(),
		.fileOnDisk = (copy->photo || copy->document) && FileOnDisk(*copy),
		.dropCaption = dropCaption,
	});
	switch (copy->plan.kind) {
	case CopyKind::Skip:
		LOG(("MZGram: protected message %1 has nothing to copy."
			).arg(item->id.bare));
		return;
	case CopyKind::Text:
		SendText(api, *copy);
		return;
	default:
		break;
	}
	if (copy->plan.download) {
		DownloadThenSend(api, copy);
	} else {
		SendFile(api, *copy);
	}
}

} // namespace

void SendProtectedCopies(
		not_null<ApiWrap*> api,
		Data::ResolvedForwardDraft &draft,
		const Api::SendAction &action) {
	if (!SaveProtectedContent()) {
		return;
	}
	const auto dropCaption = (draft.options
		== Data::ForwardOptions::NoNamesAndCaptions);
	for (auto i = begin(draft.items); i != end(draft.items);) {
		const auto item = *i;
		if (!item->isService() && SendAsCopy(ProtectedOnServer(item))) {
			SendCopy(api, item, action, dropCaption);
			i = draft.items.erase(i);
		} else {
			++i;
		}
	}
}

} // namespace MZGram
