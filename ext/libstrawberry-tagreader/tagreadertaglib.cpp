/* This file is part of Strawberry.
   Copyright 2013, David Sansome <me@davidsansome.com>
   Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include "tagreadertaglib.h"

#include <string>
#include <memory>
#include <algorithm>
#include <sys/stat.h>

#include <taglib/taglib.h>
#include <taglib/fileref.h>
#include <taglib/tbytevector.h>
#include <taglib/tfile.h>
#include <taglib/tlist.h>
#include <taglib/tstring.h>
#include <taglib/tstringlist.h>
#include <taglib/tpropertymap.h>
#include <taglib/audioproperties.h>
#include <taglib/xiphcomment.h>
#include <taglib/tag.h>
#include <taglib/apetag.h>
#include <taglib/apeitem.h>
#include <taglib/apeproperties.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/unsynchronizedlyricsframe.h>
#include <taglib/popularimeterframe.h>
#include <taglib/uniquefileidentifierframe.h>
#include <taglib/commentsframe.h>
#include <taglib/flacfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/flacproperties.h>
#include <taglib/flacpicture.h>
#include <taglib/vorbisfile.h>
#include <taglib/speexfile.h>
#include <taglib/wavfile.h>
#include <taglib/wavpackfile.h>
#include <taglib/wavpackproperties.h>
#include <taglib/aifffile.h>
#include <taglib/asffile.h>
#include <taglib/asftag.h>
#include <taglib/asfattribute.h>
#include <taglib/asfproperties.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4item.h>
#include <taglib/mp4coverart.h>
#include <taglib/mp4properties.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
#include <taglib/opusfile.h>
#include <taglib/trueaudiofile.h>
#include <taglib/apefile.h>
#include <taglib/modfile.h>
#include <taglib/s3mfile.h>
#include <taglib/xmfile.h>
#include <taglib/itfile.h>
#ifdef HAVE_TAGLIB_DSFFILE
#  include <taglib/dsffile.h>
#endif
#ifdef HAVE_TAGLIB_DSDIFFFILE
#  include <taglib/dsdifffile.h>
#endif

#include <QtGlobal>
#include <QVector>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QDateTime>
#include <QtDebug>

#include "core/logging.h"
#include "core/messagehandler.h"
#include "utilities/timeconstants.h"

#undef TStringToQString
#undef QStringToTString

namespace {

constexpr char kID3v2_AcoustID_ID[] = "Acoustid Id";
constexpr char kID3v2_AcoustID_Fingerprint[] = "Acoustid Fingerprint";
constexpr char kID3v2_MusicBrainz_AlbumArtistID[] = "MusicBrainz Album Artist Id";
constexpr char kID3v2_MusicBrainz_ArtistID[] = "MusicBrainz Artist Id";
constexpr char kID3v2_MusicBrainz_OriginalArtistID[] = "MusicBrainz Original Artist Id";
constexpr char kID3v2_MusicBrainz_AlbumID[] = "MusicBrainz Album Id";
constexpr char kID3v2_MusicBrainz_OriginalAlbumID[] = "MusicBrainz Original Album Id";
constexpr char kID3v2_MusicBrainz_RecordingID[] = "MUSICBRAINZ_TRACKID";
constexpr char kID3v2_MusicBrainz_TrackID[] = "MusicBrainz Release Track Id";
constexpr char kID3v2_MusicBrainz_DiscID[] = "MusicBrainz Disc Id";
constexpr char kID3v2_MusicBrainz_ReleaseGroupID[] = "MusicBrainz Release Group Id";
constexpr char kID3v2_MusicBrainz_WorkID[] = "MusicBrainz Work Id";

constexpr char kMP4_OriginalYear_ID[] = "----:com.apple.iTunes:ORIGINAL YEAR";
constexpr char kMP4_FMPS_Playcount_ID[] = "----:com.apple.iTunes:FMPS_Playcount";
constexpr char kMP4_FMPS_Rating_ID[] = "----:com.apple.iTunes:FMPS_Rating";
constexpr char kMP4_AcoustID_ID[] = "----:com.apple.iTunes:Acoustid Id";
constexpr char kMP4_AcoustID_Fingerprint[] = "----:com.apple.iTunes:Acoustid Fingerprint";
constexpr char kMP4_MusicBrainz_AlbumArtistID[] = "----:com.apple.iTunes:MusicBrainz Album Artist Id";
constexpr char kMP4_MusicBrainz_ArtistID[] = "----:com.apple.iTunes:MusicBrainz Artist Id";
constexpr char kMP4_MusicBrainz_OriginalArtistID[] = "----:com.apple.iTunes:MusicBrainz Original Artist Id";
constexpr char kMP4_MusicBrainz_AlbumID[] = "----:com.apple.iTunes:MusicBrainz Album Id";
constexpr char kMP4_MusicBrainz_OriginalAlbumID[] = "----:com.apple.iTunes:MusicBrainz Original Album Id";
constexpr char kMP4_MusicBrainz_RecordingID[] = "----:com.apple.iTunes:MusicBrainz Track Id";
constexpr char kMP4_MusicBrainz_TrackID[] = "----:com.apple.iTunes:MusicBrainz Release Track Id";
constexpr char kMP4_MusicBrainz_DiscID[] = "----:com.apple.iTunes:MusicBrainz Disc Id";
constexpr char kMP4_MusicBrainz_ReleaseGroupID[] = "----:com.apple.iTunes:MusicBrainz Release Group Id";
constexpr char kMP4_MusicBrainz_WorkID[] = "----:com.apple.iTunes:MusicBrainz Work Id";

constexpr char kASF_OriginalDate_ID[] = "WM/OriginalReleaseTime";
constexpr char kASF_OriginalYear_ID[] = "WM/OriginalReleaseYear";
constexpr char kASF_AcoustID_ID[] = "Acoustid/Id";
constexpr char kASF_AcoustID_Fingerprint[] = "Acoustid/Fingerprint";
constexpr char kASF_MusicBrainz_AlbumArtistID[] = "MusicBrainz/Album Artist Id";
constexpr char kASF_MusicBrainz_ArtistID[] = "MusicBrainz/Artist Id";
constexpr char kASF_MusicBrainz_OriginalArtistID[] = "MusicBrainz/Original Artist Id";
constexpr char kASF_MusicBrainz_AlbumID[] = "MusicBrainz/Album Id";
constexpr char kASF_MusicBrainz_OriginalAlbumID[] = "MusicBrainz/Original Album Id";
constexpr char kASF_MusicBrainz_RecordingID[] = "MusicBrainz/Track Id";
constexpr char kASF_MusicBrainz_TrackID[] = "MusicBrainz/Release Track Id";
constexpr char kASF_MusicBrainz_DiscID[] = "MusicBrainz/Disc Id";
constexpr char kASF_MusicBrainz_ReleaseGroupID[] = "MusicBrainz/Release Group Id";
constexpr char kASF_MusicBrainz_WorkID[] = "MusicBrainz/Work Id";

}  // namespace

class FileRefFactory {
 public:
  FileRefFactory() = default;
  virtual ~FileRefFactory() = default;
  virtual TagLib::FileRef *GetFileRef(const QString &filename) = 0;

 private:
  Q_DISABLE_COPY(FileRefFactory)
};

class TagLibFileRefFactory : public FileRefFactory {
 public:
  TagLibFileRefFactory() = default;
  TagLib::FileRef *GetFileRef(const QString &filename) override {
#ifdef Q_OS_WIN32
    return new TagLib::FileRef(filename.toStdWString().c_str());
#else
    return new TagLib::FileRef(QFile::encodeName(filename).constData());
#endif
  }

 private:
  Q_DISABLE_COPY(TagLibFileRefFactory)
};

TagReaderTagLib::TagReaderTagLib() : factory_(new TagLibFileRefFactory) {}

TagReaderTagLib::~TagReaderTagLib() {
  delete factory_;
}

bool TagReaderTagLib::IsMediaFile(const QString &filename) const {

  qLog(Debug) << "Checking for valid file" << filename;

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  return fileref && !fileref->isNull() && fileref->file() && fileref->tag();

}

spb::tagreader::SongMetadata_FileType TagReaderTagLib::GuessFileType(TagLib::FileRef *fileref) const {

  if (dynamic_cast<TagLib::RIFF::WAV::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_WAV;
  if (dynamic_cast<TagLib::FLAC::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_FLAC;
  if (dynamic_cast<TagLib::WavPack::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_WAVPACK;
  if (dynamic_cast<TagLib::Ogg::FLAC::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_OGGFLAC;
  if (dynamic_cast<TagLib::Ogg::Vorbis::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_OGGVORBIS;
  if (dynamic_cast<TagLib::Ogg::Opus::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_OGGOPUS;
  if (dynamic_cast<TagLib::Ogg::Speex::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_OGGSPEEX;
  if (dynamic_cast<TagLib::MPEG::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_MPEG;
  if (dynamic_cast<TagLib::MP4::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_MP4;
  if (dynamic_cast<TagLib::ASF::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_ASF;
  if (dynamic_cast<TagLib::RIFF::AIFF::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_AIFF;
  if (dynamic_cast<TagLib::MPC::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_MPC;
  if (dynamic_cast<TagLib::TrueAudio::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_TRUEAUDIO;
  if (dynamic_cast<TagLib::APE::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_APE;
  if (dynamic_cast<TagLib::Mod::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_MOD;
  if (dynamic_cast<TagLib::S3M::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_S3M;
  if (dynamic_cast<TagLib::XM::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_XM;
  if (dynamic_cast<TagLib::IT::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_IT;
#ifdef HAVE_TAGLIB_DSFFILE
  if (dynamic_cast<TagLib::DSF::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_DSF;
#endif
#ifdef HAVE_TAGLIB_DSDIFFFILE
  if (dynamic_cast<TagLib::DSDIFF::File*>(fileref->file())) return spb::tagreader::SongMetadata_FileType_DSDIFF;
#endif

  return spb::tagreader::SongMetadata_FileType_UNKNOWN;

}

TagReaderBase::Result TagReaderTagLib::ReadFile(const QString &filename, spb::tagreader::SongMetadata *song) const {

  if (filename.isEmpty()) {
    return Result::ErrorCode::FilenameMissing;
  }

  qLog(Debug) << "Reading tags from" << filename;

  const QFileInfo fileinfo(filename);
  if (!fileinfo.exists()) {
    qLog(Error) << "File" << filename << "does not exist";
    return Result::ErrorCode::FileDoesNotExist;
  }

  const QByteArray url = QUrl::fromLocalFile(filename).toEncoded();
  const QByteArray basefilename = fileinfo.fileName().toUtf8();
  song->set_basefilename(basefilename.constData(), basefilename.length());
  song->set_url(url.constData(), url.size());
  song->set_filesize(fileinfo.size());
  song->set_mtime(fileinfo.lastModified().isValid() ? std::max(fileinfo.lastModified().toSecsSinceEpoch(), 0LL) : 0LL);
  song->set_ctime(fileinfo.birthTime().isValid() ? std::max(fileinfo.birthTime().toSecsSinceEpoch(), 0LL) : fileinfo.lastModified().isValid() ? std::max(fileinfo.lastModified().toSecsSinceEpoch(), 0LL) : 0LL);
  if (song->ctime() <= 0) {
    song->set_ctime(song->mtime());
  }
  song->set_lastseen(QDateTime::currentSecsSinceEpoch());

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return Result::ErrorCode::FileOpenError;
  }

  song->set_filetype(GuessFileType(fileref.get()));

  if (fileref->audioProperties()) {
    song->set_bitrate(fileref->audioProperties()->bitrate());
    song->set_samplerate(fileref->audioProperties()->sampleRate());
    song->set_length_nanosec(fileref->audioProperties()->lengthInMilliseconds() * kNsecPerMsec);
  }

  TagLib::Tag *tag = fileref->tag();
  if (tag) {
    AssignTagLibStringToStdString(tag->title(), song->mutable_title());
    AssignTagLibStringToStdString(tag->artist(), song->mutable_artist());  // TPE1
    AssignTagLibStringToStdString(tag->album(), song->mutable_album());
    AssignTagLibStringToStdString(tag->genre(), song->mutable_genre());
    song->set_year(static_cast<int>(tag->year()));
    song->set_track(static_cast<int>(tag->track()));
    song->set_valid(true);
  }

  QString disc;
  QString compilation;
  QString lyrics;

  // Handle all the files which have VorbisComments (Ogg, OPUS, ...) in the same way;
  // apart, so we keep specific behavior for some formats by adding another "else if" block below.
  if (TagLib::Ogg::XiphComment *xiph_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    ParseOggTag(xiph_comment->fieldListMap(), &disc, &compilation, song);
    TagLib::List<TagLib::FLAC::Picture*> pictures = xiph_comment->pictureList();
    if (!pictures.isEmpty()) {
      for (TagLib::FLAC::Picture *picture : pictures) {
        if (picture->type() == TagLib::FLAC::Picture::FrontCover && picture->data().size() > 0) {
          song->set_art_embedded(true);
          break;
        }
      }
    }
  }

  if (TagLib::FLAC::File *file_flac = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    song->set_bitdepth(file_flac->audioProperties()->bitsPerSample());
    if (file_flac->xiphComment()) {
      ParseOggTag(file_flac->xiphComment()->fieldListMap(), &disc, &compilation, song);
      TagLib::List<TagLib::FLAC::Picture*> pictures = file_flac->pictureList();
      if (!pictures.isEmpty()) {
        for (TagLib::FLAC::Picture *picture : pictures) {
          if (picture->type() == TagLib::FLAC::Picture::FrontCover && picture->data().size() > 0) {
            song->set_art_embedded(true);
            break;
          }
        }
      }
    }
    if (tag) AssignTagLibStringToStdString(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::WavPack::File *file_wavpack = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    song->set_bitdepth(file_wavpack->audioProperties()->bitsPerSample());
    if (file_wavpack->APETag()) {
      ParseAPETag(file_wavpack->APETag()->itemListMap(), &disc, &compilation, song);
    }
    if (tag) AssignTagLibStringToStdString(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::APE::File *file_ape = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    if (file_ape->APETag()) {
      ParseAPETag(file_ape->APETag()->itemListMap(), &disc, &compilation, song);
    }
    song->set_bitdepth(file_ape->audioProperties()->bitsPerSample());
    if (tag) AssignTagLibStringToStdString(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::MPEG::File *file_mpeg = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    if (file_mpeg->hasID3v2Tag()) {
      ParseID3v2Tag(file_mpeg->ID3v2Tag(), &disc, &compilation, song);
    }
  }

  else if (TagLib::MP4::File *file_mp4 = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {

    song->set_bitdepth(file_mp4->audioProperties()->bitsPerSample());

    if (file_mp4->tag()) {
      TagLib::MP4::Tag *mp4_tag = file_mp4->tag();

      // Find album artists
      if (mp4_tag->item("aART").isValid()) {
        TagLib::StringList album_artists = mp4_tag->item("aART").toStringList();
        if (!album_artists.isEmpty()) {
          AssignTagLibStringToStdString(album_artists.front(), song->mutable_albumartist());
        }
      }

      // Find album cover art
      if (mp4_tag->item("covr").isValid()) {
        song->set_art_embedded(true);
      }

      if (mp4_tag->item("disk").isValid()) {
        disc = TagLibStringToQString(TagLib::String::number(mp4_tag->item("disk").toIntPair().first));
      }

      if (mp4_tag->item("\251wrt").isValid()) {
        AssignTagLibStringToStdString(mp4_tag->item("\251wrt").toStringList().toString(", "), song->mutable_composer());
      }
      if (mp4_tag->item("\251grp").isValid()) {
        AssignTagLibStringToStdString(mp4_tag->item("\251grp").toStringList().toString(" "), song->mutable_grouping());
      }
      if (mp4_tag->item("\251lyr").isValid()) {
        AssignTagLibStringToStdString(mp4_tag->item("\251lyr").toStringList().toString(" "), song->mutable_lyrics());
      }

      if (mp4_tag->item(kMP4_OriginalYear_ID).isValid()) {
        song->set_originalyear(TagLibStringToQString(mp4_tag->item(kMP4_OriginalYear_ID).toStringList().toString('\n')).left(4).toInt());
      }

      if (mp4_tag->item("cpil").isValid()) {
        song->set_compilation(mp4_tag->item("cpil").toBool());
      }

      {
        TagLib::MP4::Item item = mp4_tag->item(kMP4_FMPS_Playcount_ID);
        if (item.isValid()) {
          const int playcount = TagLibStringToQString(item.toStringList().toString('\n')).toInt();
          if (song->playcount() <= 0 && playcount > 0) {
            song->set_playcount(static_cast<uint>(playcount));
          }
        }
      }

      {
        TagLib::MP4::Item item = mp4_tag->item(kMP4_FMPS_Rating_ID);
        if (item.isValid()) {
          const float rating = TagLibStringToQString(item.toStringList().toString('\n')).toFloat();
          if (song->rating() <= 0 && rating > 0) {
            song->set_rating(rating);
          }
        }
      }

      AssignTagLibStringToStdString(mp4_tag->comment(), song->mutable_comment());

      if (mp4_tag->contains(kMP4_AcoustID_ID)) {
        AssignTagLibStringToStdString(mp4_tag->item(kMP4_AcoustID_ID).toStringList().toString(), song->mutable_acoustid_id());
      }
      if (mp4_tag->contains(kMP4_AcoustID_Fingerprint)) {
        AssignTagLibStringToStdString(mp4_tag->item(kMP4_AcoustID_Fingerprint).toStringList().toString(), song->mutable_acoustid_fingerprint());
      }

      if (mp4_tag->contains(kMP4_MusicBrainz_AlbumArtistID)) {
        AssignTagLibStringToStdString(mp4_tag->item(kMP4_MusicBrainz_AlbumArtistID).toStringList().toString(), song->mutable_musicbrainz_album_artist_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_ArtistID)) {
        AssignTagLibStringToStdString(mp4_tag->item(kMP4_MusicBrainz_ArtistID).toStringList().toString(), song->mutable_musicbrainz_artist_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_OriginalArtistID)) {
        AssignTagLibStringToStdString(mp4_tag->item(kMP4_MusicBrainz_OriginalArtistID).toStringList().toString(), song->mutable_musicbrainz_original_artist_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_AlbumID)) {
        AssignTagLibStringToStdString(mp4_tag->item(kMP4_MusicBrainz_AlbumID).toStringList().toString(), song->mutable_musicbrainz_album_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_OriginalAlbumID)) {
        AssignTagLibStringToStdString(mp4_tag->item(kMP4_MusicBrainz_OriginalAlbumID).toStringList().toString(), song->mutable_musicbrainz_original_album_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_RecordingID)) {
        AssignTagLibStringToStdString(mp4_tag->item(kMP4_MusicBrainz_RecordingID).toStringList().toString(), song->mutable_musicbrainz_recording_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_TrackID)) {
        AssignTagLibStringToStdString(mp4_tag->item(kMP4_MusicBrainz_TrackID).toStringList().toString(), song->mutable_musicbrainz_track_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_DiscID)) {
        AssignTagLibStringToStdString(mp4_tag->item(kMP4_MusicBrainz_DiscID).toStringList().toString(), song->mutable_musicbrainz_disc_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_ReleaseGroupID)) {
        AssignTagLibStringToStdString(mp4_tag->item(kMP4_MusicBrainz_ReleaseGroupID).toStringList().toString(), song->mutable_musicbrainz_release_group_id());
      }
      if (mp4_tag->contains(kMP4_MusicBrainz_WorkID)) {
        AssignTagLibStringToStdString(mp4_tag->item(kMP4_MusicBrainz_WorkID).toStringList().toString(), song->mutable_musicbrainz_work_id());
      }

    }
  }

  else if (TagLib::ASF::File *file_asf = dynamic_cast<TagLib::ASF::File*>(fileref->file())) {

    song->set_bitdepth(file_asf->audioProperties()->bitsPerSample());

    if (file_asf->tag()) {
      AssignTagLibStringToStdString(file_asf->tag()->comment(), song->mutable_comment());

      const TagLib::ASF::AttributeListMap &attributes_map = file_asf->tag()->attributeListMap();

      if (attributes_map.contains(kASF_OriginalDate_ID)) {
        const TagLib::ASF::AttributeList &attributes = attributes_map[kASF_OriginalDate_ID];
        if (!attributes.isEmpty()) {
          song->set_originalyear(TagLibStringToQString(attributes.front().toString()).left(4).toInt());
        }
      }
      else if (attributes_map.contains(kASF_OriginalYear_ID)) {
        const TagLib::ASF::AttributeList &attributes = attributes_map[kASF_OriginalYear_ID];
        if (!attributes.isEmpty()) {
          song->set_originalyear(TagLibStringToQString(attributes.front().toString()).left(4).toInt());
        }
      }

      if (attributes_map.contains("FMPS/Playcount")) {
        const TagLib::ASF::AttributeList &attributes = attributes_map["FMPS/Playcount"];
        if (!attributes.isEmpty()) {
          int playcount = TagLibStringToQString(attributes.front().toString()).toInt();
          if (song->playcount() <= 0 && playcount > 0) {
            song->set_playcount(static_cast<uint>(playcount));
          }
        }
      }

      if (attributes_map.contains("FMPS/Rating")) {
        const TagLib::ASF::AttributeList &attributes = attributes_map["FMPS/Rating"];
        if (!attributes.isEmpty()) {
          float rating = TagLibStringToQString(attributes.front().toString()).toFloat();
          if (song->rating() <= 0 && rating > 0) {
            song->set_rating(rating);
          }
        }
      }

      if (attributes_map.contains(kASF_AcoustID_ID)) {
        AssignTagLibStringToStdString(attributes_map[kASF_AcoustID_ID].front().toString(), song->mutable_acoustid_id());
      }
      if (attributes_map.contains(kASF_AcoustID_Fingerprint)) {
        AssignTagLibStringToStdString(attributes_map[kASF_AcoustID_Fingerprint].front().toString(), song->mutable_acoustid_fingerprint());
      }

      if (attributes_map.contains(kASF_MusicBrainz_AlbumArtistID)) {
        AssignTagLibStringToStdString(attributes_map[kASF_MusicBrainz_AlbumArtistID].front().toString(), song->mutable_musicbrainz_album_artist_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_ArtistID)) {
        AssignTagLibStringToStdString(attributes_map[kASF_MusicBrainz_ArtistID].front().toString(), song->mutable_musicbrainz_artist_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_OriginalArtistID)) {
        AssignTagLibStringToStdString(attributes_map[kASF_MusicBrainz_OriginalArtistID].front().toString(), song->mutable_musicbrainz_original_artist_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_AlbumID)) {
        AssignTagLibStringToStdString(attributes_map[kASF_MusicBrainz_AlbumID].front().toString(), song->mutable_musicbrainz_album_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_OriginalAlbumID)) {
        AssignTagLibStringToStdString(attributes_map[kASF_MusicBrainz_OriginalAlbumID].front().toString(), song->mutable_musicbrainz_original_album_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_RecordingID)) {
        AssignTagLibStringToStdString(attributes_map[kASF_MusicBrainz_RecordingID].front().toString(), song->mutable_musicbrainz_recording_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_TrackID)) {
        AssignTagLibStringToStdString(attributes_map[kASF_MusicBrainz_TrackID].front().toString(), song->mutable_musicbrainz_track_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_DiscID)) {
        AssignTagLibStringToStdString(attributes_map[kASF_MusicBrainz_DiscID].front().toString(), song->mutable_musicbrainz_disc_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_ReleaseGroupID)) {
        AssignTagLibStringToStdString(attributes_map[kASF_MusicBrainz_ReleaseGroupID].front().toString(), song->mutable_musicbrainz_release_group_id());
      }
      if (attributes_map.contains(kASF_MusicBrainz_WorkID)) {
        AssignTagLibStringToStdString(attributes_map[kASF_MusicBrainz_WorkID].front().toString(), song->mutable_musicbrainz_work_id());
      }

    }

  }

  else if (TagLib::MPC::File *file_mpc = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    if (file_mpc->APETag()) {
      ParseAPETag(file_mpc->APETag()->itemListMap(), &disc, &compilation, song);
    }
    if (tag) AssignTagLibStringToStdString(tag->comment(), song->mutable_comment());
  }

  else if (TagLib::RIFF::WAV::File *file_wav = dynamic_cast<TagLib::RIFF::WAV::File*>(fileref->file())) {
    if (file_wav->hasID3v2Tag()) {
      ParseID3v2Tag(file_wav->ID3v2Tag(), &disc, &compilation, song);
    }
  }

  else if (tag) {
    AssignTagLibStringToStdString(tag->comment(), song->mutable_comment());
  }

  if (!disc.isEmpty()) {
    const qint64 i = disc.indexOf(QLatin1Char('/'));
    if (i != -1) {
      // disc.right( i ).toInt() is total number of discs, we don't use this at the moment
      song->set_disc(disc.left(i).toInt());
    }
    else {
      song->set_disc(disc.toInt());
    }
  }

  if (compilation.isEmpty()) {
    // well, it wasn't set, but if the artist is VA assume it's a compilation
    const QString albumartist = QString::fromStdString(song->albumartist());
    const QString artist = QString::fromStdString(song->artist());
    if (artist.compare(QLatin1String("various artists")) == 0 || albumartist.compare(QLatin1String("various artists")) == 0) {
      song->set_compilation(true);
    }
  }
  else {
    song->set_compilation(compilation.toInt() == 1);
  }

  if (!lyrics.isEmpty()) song->set_lyrics(lyrics.toStdString());

  // Set integer fields to -1 if they're not valid

  if (song->track() <= 0) { song->set_track(-1); }
  if (song->disc() <= 0) { song->set_disc(-1); }
  if (song->year() <= 0) { song->set_year(-1); }
  if (song->originalyear() <= 0) { song->set_originalyear(-1); }
  if (song->samplerate() <= 0) { song->set_samplerate(-1); }
  if (song->bitdepth() <= 0) { song->set_bitdepth(-1); }
  if (song->bitrate() <= 0) { song->set_bitrate(-1); }
  if (song->lastplayed() <= 0) { song->set_lastplayed(-1); }

  if (song->filetype() == spb::tagreader::SongMetadata_FileType_UNKNOWN) {
    qLog(Error) << "Unknown audio filetype reading" << filename;
    return Result::ErrorCode::Unsupported;
  }

  qLog(Debug) << "Got tags for" << filename;

  return Result::ErrorCode::Success;

}

void TagReaderTagLib::ParseID3v2Tag(TagLib::ID3v2::Tag *tag, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const {

  TagLib::ID3v2::FrameListMap map = tag->frameListMap();

  if (map.contains("TPOS")) *disc = TagLibStringToQString(map["TPOS"].front()->toString()).trimmed();
  if (map.contains("TCOM")) AssignTagLibStringToStdString(map["TCOM"].front()->toString(), song->mutable_composer());

  // content group
  if (map.contains("TIT1")) AssignTagLibStringToStdString(map["TIT1"].front()->toString(), song->mutable_grouping());

  // original artist/performer
  if (map.contains("TOPE")) AssignTagLibStringToStdString(map["TOPE"].front()->toString(), song->mutable_performer());

  // Skip TPE1 (which is the artist) here because we already fetched it

  // non-standard: Apple, Microsoft
  if (map.contains("TPE2")) AssignTagLibStringToStdString(map["TPE2"].front()->toString(), song->mutable_albumartist());

  if (map.contains("TCMP")) *compilation = TagLibStringToQString(map["TCMP"].front()->toString()).trimmed();

  if (map.contains("TDOR")) {
    song->set_originalyear(map["TDOR"].front()->toString().substr(0, 4).toInt());
  }
  else if (map.contains("TORY")) {
    song->set_originalyear(map["TORY"].front()->toString().substr(0, 4).toInt());
  }

  if (map.contains("USLT")) {
    AssignTagLibStringToStdString(map["USLT"].front()->toString(), song->mutable_lyrics());
  }
  else if (map.contains("SYLT")) {
    AssignTagLibStringToStdString(map["SYLT"].front()->toString(), song->mutable_lyrics());
  }

  if (map.contains("APIC")) song->set_art_embedded(true);

  // Find a suitable comment tag.  For now we ignore iTunNORM comments.
  for (uint i = 0; i < map["COMM"].size(); ++i) {
    const TagLib::ID3v2::CommentsFrame *frame = dynamic_cast<const TagLib::ID3v2::CommentsFrame*>(map["COMM"][i]);

    if (frame && TagLibStringToQString(frame->description()) != QLatin1String("iTunNORM")) {
      AssignTagLibStringToStdString(frame->text(), song->mutable_comment());
      break;
    }
  }

  if (TagLib::ID3v2::UserTextIdentificationFrame *frame_fmps_playcount = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "FMPS_Playcount")) {
    TagLib::StringList frame_field_list = frame_fmps_playcount->fieldList();
    if (frame_field_list.size() > 1) {
      int playcount = TagLibStringToQString(frame_field_list[1]).toInt();
      if (song->playcount() <= 0 && playcount > 0) {
        song->set_playcount(playcount);
      }
    }
  }

  if (TagLib::ID3v2::UserTextIdentificationFrame *frame_fmps_rating = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, "FMPS_Rating")) {
    TagLib::StringList frame_field_list = frame_fmps_rating->fieldList();
    if (frame_field_list.size() > 1) {
      float rating = TagLibStringToQString(frame_field_list[1]).toFloat();
      if (song->rating() <= 0 && rating > 0 && rating <= 1.0) {
        song->set_rating(rating);
      }
    }
  }

  if (map.contains("POPM")) {
    const TagLib::ID3v2::PopularimeterFrame *frame = dynamic_cast<const TagLib::ID3v2::PopularimeterFrame*>(map["POPM"].front());
    if (frame) {
      if (song->playcount() <= 0 && frame->counter() > 0) {
        song->set_playcount(frame->counter());
      }
      if (song->rating() <= 0 && frame->rating() > 0) {
        song->set_rating(ConvertPOPMRating(frame->rating()));
      }
    }
  }

  if (map.contains("UFID")) {
    for (uint i = 0; i < map["UFID"].size(); ++i) {
      if (TagLib::ID3v2::UniqueFileIdentifierFrame *frame = dynamic_cast<TagLib::ID3v2::UniqueFileIdentifierFrame*>(map["UFID"][i])) {
        const TagLib::PropertyMap property_map = frame->asProperties();
        if (property_map.contains(kID3v2_MusicBrainz_RecordingID)) {
          AssignTagLibStringToStdString(property_map[kID3v2_MusicBrainz_RecordingID].toString(), song->mutable_musicbrainz_recording_id());
        }
      }
    }
  }

  if (map.contains("TXXX")) {
    for (uint i = 0; i < map["TXXX"].size(); ++i) {
      if (TagLib::ID3v2::UserTextIdentificationFrame *frame = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(map["TXXX"][i])) {
        const TagLib::StringList frame_field_list = frame->fieldList();
        if (frame_field_list.size() != 2) continue;
        if (frame->description() == kID3v2_AcoustID_ID) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_acoustid_id());
        }
        if (frame->description() == kID3v2_AcoustID_Fingerprint) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_acoustid_fingerprint());
        }
        if (frame->description() == kID3v2_MusicBrainz_AlbumArtistID) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_album_artist_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_ArtistID) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_artist_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_OriginalArtistID) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_original_artist_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_AlbumID) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_album_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_OriginalAlbumID) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_original_album_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_TrackID) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_track_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_DiscID) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_disc_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_ReleaseGroupID) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_release_group_id());
        }
        if (frame->description() == kID3v2_MusicBrainz_WorkID) {
          AssignTagLibStringToStdString(frame_field_list.back(), song->mutable_musicbrainz_work_id());
        }
      }
    }
  }

}

void TagReaderTagLib::ParseOggTag(const TagLib::Ogg::FieldListMap &map, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const {

  if (map.contains("COMPOSER")) AssignTagLibStringToStdString(map["COMPOSER"].front(), song->mutable_composer());
  if (map.contains("PERFORMER")) AssignTagLibStringToStdString(map["PERFORMER"].front(), song->mutable_performer());
  if (map.contains("CONTENT GROUP")) AssignTagLibStringToStdString(map["CONTENT GROUP"].front(), song->mutable_grouping());
  if (map.contains("GROUPING")) AssignTagLibStringToStdString(map["GROUPING"].front(), song->mutable_grouping());

  if (map.contains("ALBUMARTIST")) AssignTagLibStringToStdString(map["ALBUMARTIST"].front(), song->mutable_albumartist());
  else if (map.contains("ALBUM ARTIST")) AssignTagLibStringToStdString(map["ALBUM ARTIST"].front(), song->mutable_albumartist());

  if (map.contains("ORIGINALDATE")) song->set_originalyear(TagLibStringToQString(map["ORIGINALDATE"].front()).left(4).toInt());
  else if (map.contains("ORIGINALYEAR")) song->set_originalyear(TagLibStringToQString(map["ORIGINALYEAR"].front()).toInt());

  if (map.contains("DISCNUMBER")) *disc = TagLibStringToQString(map["DISCNUMBER"].front()).trimmed();
  if (map.contains("COMPILATION")) *compilation = TagLibStringToQString(map["COMPILATION"].front()).trimmed();
  if (map.contains("COVERART")) song->set_art_embedded(true);
  if (map.contains("METADATA_BLOCK_PICTURE")) song->set_art_embedded(true);

  if (map.contains("FMPS_PLAYCOUNT") && song->playcount() <= 0) {
    const int playcount = TagLibStringToQString(map["FMPS_PLAYCOUNT"].front()).trimmed().toInt();
    song->set_playcount(static_cast<uint>(playcount));
  }
  if (map.contains("FMPS_RATING") && song->rating() <= 0) song->set_rating(TagLibStringToQString(map["FMPS_RATING"].front()).trimmed().toFloat());

  if (map.contains("LYRICS")) AssignTagLibStringToStdString(map["LYRICS"].front(), song->mutable_lyrics());
  else if (map.contains("UNSYNCEDLYRICS")) AssignTagLibStringToStdString(map["UNSYNCEDLYRICS"].front(), song->mutable_lyrics());

  if (map.contains("ACOUSTID_ID")) AssignTagLibStringToStdString(map["ACOUSTID_ID"].front(), song->mutable_acoustid_id());
  if (map.contains("ACOUSTID_FINGERPRINT")) AssignTagLibStringToStdString(map["ACOUSTID_FINGERPRINT"].front(), song->mutable_acoustid_fingerprint());

  if (map.contains("MUSICBRAINZ_ALBUMARTISTID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_ALBUMARTISTID"].front(), song->mutable_musicbrainz_album_artist_id());
  if (map.contains("MUSICBRAINZ_ARTISTID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_ARTISTID"].front(), song->mutable_musicbrainz_artist_id());
  if (map.contains("MUSICBRAINZ_ORIGINALARTISTID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_ORIGINALARTISTID"].front(), song->mutable_musicbrainz_original_artist_id());
  if (map.contains("MUSICBRAINZ_ALBUMID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_ALBUMID"].front(), song->mutable_musicbrainz_album_id());
  if (map.contains("MUSICBRAINZ_ORIGINALALBUMID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_ORIGINALALBUMID"].front(), song->mutable_musicbrainz_original_album_id());
  if (map.contains("MUSICBRAINZ_TRACKID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_TRACKID"].front(), song->mutable_musicbrainz_recording_id());
  if (map.contains("MUSICBRAINZ_RELEASETRACKID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_RELEASETRACKID"].front(), song->mutable_musicbrainz_track_id());
  if (map.contains("MUSICBRAINZ_DISCID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_DISCID"].front(), song->mutable_musicbrainz_disc_id());
  if (map.contains("MUSICBRAINZ_RELEASEGROUPID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_RELEASEGROUPID"].front(), song->mutable_musicbrainz_release_group_id());
  if (map.contains("MUSICBRAINZ_WORKID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_WORKID"].front(), song->mutable_musicbrainz_work_id());

}

void TagReaderTagLib::ParseAPETag(const TagLib::APE::ItemListMap &map, QString *disc, QString *compilation, spb::tagreader::SongMetadata *song) const {

  TagLib::APE::ItemListMap::ConstIterator it = map.find("ALBUM ARTIST");
  if (it != map.end()) {
    TagLib::StringList album_artists = it->second.values();
    if (!album_artists.isEmpty()) {
      AssignTagLibStringToStdString(album_artists.front(), song->mutable_albumartist());
    }
  }

  if (map.find("COVER ART (FRONT)") != map.end()) song->set_art_embedded(true);
  if (map.contains("COMPILATION")) {
    *compilation = TagLibStringToQString(TagLib::String::number(map["COMPILATION"].toString().toInt()));
  }

  if (map.contains("DISC")) {
    *disc = TagLibStringToQString(TagLib::String::number(map["DISC"].toString().toInt()));
  }

  if (map.contains("PERFORMER")) {
    AssignTagLibStringToStdString(map["PERFORMER"].values().toString(", "), song->mutable_performer());
  }

  if (map.contains("COMPOSER")) {
    AssignTagLibStringToStdString(map["COMPOSER"].values().toString(", "), song->mutable_composer());
  }

  if (map.contains("GROUPING")) {
    AssignTagLibStringToStdString(map["GROUPING"].values().toString(" "), song->mutable_grouping());
  }

  if (map.contains("LYRICS")) {
    AssignTagLibStringToStdString(map["LYRICS"].toString(), song->mutable_lyrics());
  }

  if (map.contains("FMPS_PLAYCOUNT")) {
    const int playcount = TagLibStringToQString(map["FMPS_PLAYCOUNT"].toString()).toInt();
    if (song->playcount() <= 0 && playcount > 0) {
      song->set_playcount(static_cast<uint>(playcount));
    }
  }

  if (map.contains("FMPS_RATING")) {
    const float rating = TagLibStringToQString(map["FMPS_RATING"].toString()).toFloat();
    if (song->rating() <= 0 && rating > 0) {
      song->set_rating(rating);
    }
  }

  if (map.contains("ACOUSTID_ID")) AssignTagLibStringToStdString(map["ACOUSTID_ID"].toString(), song->mutable_acoustid_id());
  if (map.contains("ACOUSTID_FINGERPRINT")) AssignTagLibStringToStdString(map["ACOUSTID_FINGERPRINT"].toString(), song->mutable_acoustid_fingerprint());

  if (map.contains("MUSICBRAINZ_ALBUMARTISTID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_ALBUMARTISTID"].toString(), song->mutable_musicbrainz_album_artist_id());
  if (map.contains("MUSICBRAINZ_ARTISTID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_ARTISTID"].toString(), song->mutable_musicbrainz_artist_id());
  if (map.contains("MUSICBRAINZ_ORIGINALARTISTID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_ORIGINALARTISTID"].toString(), song->mutable_musicbrainz_original_artist_id());
  if (map.contains("MUSICBRAINZ_ALBUMID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_ALBUMID"].toString(), song->mutable_musicbrainz_album_id());
  if (map.contains("MUSICBRAINZ_ORIGINALALBUMID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_ORIGINALALBUMID"].toString(), song->mutable_musicbrainz_original_album_id());
  if (map.contains("MUSICBRAINZ_TRACKID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_TRACKID"].toString(), song->mutable_musicbrainz_recording_id());
  if (map.contains("MUSICBRAINZ_RELEASETRACKID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_RELEASETRACKID"].toString(), song->mutable_musicbrainz_track_id());
  if (map.contains("MUSICBRAINZ_DISCID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_DISCID"].toString(), song->mutable_musicbrainz_disc_id());
  if (map.contains("MUSICBRAINZ_RELEASEGROUPID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_RELEASEGROUPID"].toString(), song->mutable_musicbrainz_release_group_id());
  if (map.contains("MUSICBRAINZ_WORKID")) AssignTagLibStringToStdString(map["MUSICBRAINZ_WORKID"].toString(), song->mutable_musicbrainz_work_id());

}

void TagReaderTagLib::SetVorbisComments(TagLib::Ogg::XiphComment *vorbis_comment, const spb::tagreader::SongMetadata &song) const {

  vorbis_comment->addField("COMPOSER", StdStringToTagLibString(song.composer()), true);
  vorbis_comment->addField("PERFORMER", StdStringToTagLibString(song.performer()), true);
  vorbis_comment->addField("GROUPING", StdStringToTagLibString(song.grouping()), true);
  vorbis_comment->addField("DISCNUMBER", QStringToTagLibString(song.disc() <= 0 ? QString() : QString::number(song.disc())), true);
  vorbis_comment->addField("COMPILATION", QStringToTagLibString(song.compilation() ? QStringLiteral("1") : QString()), true);

  // Try to be coherent, the two forms are used but the first one is preferred

  vorbis_comment->addField("ALBUMARTIST", StdStringToTagLibString(song.albumartist()), true);
  vorbis_comment->removeFields("ALBUM ARTIST");

  vorbis_comment->addField("LYRICS", StdStringToTagLibString(song.lyrics()), true);
  vorbis_comment->removeFields("UNSYNCEDLYRICS");

}

TagReaderBase::Result TagReaderTagLib::WriteFile(const QString &filename, const spb::tagreader::WriteFileRequest &request) const {

  if (filename.isEmpty()) {
    return Result::ErrorCode::FilenameMissing;
  }

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return Result::ErrorCode::FileDoesNotExist;
  }

  const spb::tagreader::SongMetadata &song = request.metadata();
  const bool save_tags = request.has_save_tags() && request.save_tags();
  const bool save_playcount = request.has_save_playcount() && request.save_playcount();
  const bool save_rating = request.has_save_rating() && request.save_rating();
  const bool save_cover = request.has_save_cover() && request.save_cover();

  QStringList save_tags_options;
  if (save_tags) {
    save_tags_options << QStringLiteral("tags");
  }
  if (save_playcount) {
    save_tags_options << QStringLiteral("playcount");
  }
  if (save_rating) {
    save_tags_options << QStringLiteral("rating");
  }
  if (save_cover) {
    save_tags_options << QStringLiteral("embedded cover");
  }

  qLog(Debug) << "Saving" << save_tags_options.join(QLatin1String(", ")) << "to" << filename;

  const Cover cover = LoadCoverFromRequest(filename, request);

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return Result::ErrorCode::FileOpenError;
  }

  if (save_tags) {
    fileref->tag()->setTitle(song.title().empty() ? TagLib::String() : StdStringToTagLibString(song.title()));
    fileref->tag()->setArtist(song.artist().empty() ? TagLib::String() : StdStringToTagLibString(song.artist()));
    fileref->tag()->setAlbum(song.album().empty() ? TagLib::String() : StdStringToTagLibString(song.album()));
    fileref->tag()->setGenre(song.genre().empty() ? TagLib::String() : StdStringToTagLibString(song.genre()));
    fileref->tag()->setComment(song.comment().empty() ? TagLib::String() : StdStringToTagLibString(song.comment()));
    fileref->tag()->setYear(song.year() <= 0 ? 0 : song.year());
    fileref->tag()->setTrack(song.track() <= 0 ? 0 : song.track());
  }

  bool is_flac = false;
  if (TagLib::FLAC::File *file_flac = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    is_flac = true;
    TagLib::Ogg::XiphComment *xiph_comment = file_flac->xiphComment(true);
    if (xiph_comment) {
      if (save_tags) {
        SetVorbisComments(xiph_comment, song);
      }
      if (save_playcount) {
        SetPlaycount(xiph_comment, song.playcount());
      }
      if (save_rating) {
        SetRating(xiph_comment, song.rating());
      }
      if (save_cover) {
        SetEmbeddedArt(file_flac, xiph_comment, cover.data, cover.mime_type);
      }
    }
  }

  else if (TagLib::WavPack::File *file_wavpack = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file_wavpack->APETag(true);
    if (tag) {
      if (save_tags) {
        SaveAPETag(tag, song);
      }
      if (save_playcount) {
        SetPlaycount(tag, song.playcount());
      }
      if (save_rating) {
        SetRating(tag, song.rating());
      }
    }
  }

  else if (TagLib::APE::File *file_ape = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file_ape->APETag(true);
    if (tag) {
      if (save_tags) {
        SaveAPETag(tag, song);
      }
      if (save_playcount) {
        SetPlaycount(tag, song.playcount());
      }
      if (save_rating) {
        SetRating(tag, song.rating());
      }
    }
  }

  else if (TagLib::MPC::File *file_mpc = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = file_mpc->APETag(true);
    if (tag) {
      if (save_tags) {
        SaveAPETag(tag, song);
      }
      if (save_playcount) {
        SetPlaycount(tag, song.playcount());
      }
      if (save_rating) {
        SetRating(tag, song.rating());
      }
    }
  }

  else if (TagLib::MPEG::File *file_mpeg = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = file_mpeg->ID3v2Tag(true);
    if (tag) {
      if (save_tags) {
        SaveID3v2Tag(tag, song);
      }
      if (save_playcount) {
        SetPlaycount(tag, song.playcount());
      }
      if (save_rating) {
        SetRating(tag, song.rating());
      }
      if (save_cover) {
        SetEmbeddedArt(tag, cover.data, cover.mime_type);
      }
    }
  }

  else if (TagLib::MP4::File *file_mp4 = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = file_mp4->tag();
    if (tag) {
      if (save_tags) {
        tag->setItem("disk", TagLib::MP4::Item(song.disc() <= 0 - 1 ? 0 : song.disc(), 0));
        tag->setItem("\251wrt", TagLib::StringList(TagLib::String(song.composer(), TagLib::String::UTF8)));
        tag->setItem("\251grp", TagLib::StringList(TagLib::String(song.grouping(), TagLib::String::UTF8)));
        tag->setItem("\251lyr", TagLib::StringList(TagLib::String(song.lyrics(), TagLib::String::UTF8)));
        tag->setItem("aART", TagLib::StringList(TagLib::String(song.albumartist(), TagLib::String::UTF8)));
        tag->setItem("cpil", TagLib::MP4::Item(song.compilation()));
      }
      if (save_playcount) {
        SetPlaycount(tag, song.playcount());
      }
      if (save_rating) {
        SetRating(tag, song.rating());
      }
      if (save_cover) {
        SetEmbeddedArt(file_mp4, tag, cover.data, cover.mime_type);
      }
    }
  }

  else if (TagLib::RIFF::WAV::File *file_wav = dynamic_cast<TagLib::RIFF::WAV::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = file_wav->ID3v2Tag();
    if (tag) {
      if (save_tags) {
        SaveID3v2Tag(tag, song);
      }
      if (save_playcount) {
        SetPlaycount(tag, song.playcount());
      }
      if (save_rating) {
        SetRating(tag, song.rating());
      }
      if (save_cover) {
        SetEmbeddedArt(tag, cover.data, cover.mime_type);
      }
    }
  }

  // Handle all the files which have VorbisComments (Ogg, OPUS, ...) in the same way;
  // apart, so we keep specific behavior for some formats by adding another "else if" block above.
  if (!is_flac) {
    if (TagLib::Ogg::XiphComment *xiph_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
      if (xiph_comment) {
        if (save_tags) {
          SetVorbisComments(xiph_comment, song);
        }
        if (save_playcount) {
          SetPlaycount(xiph_comment, song.playcount());
        }
        if (save_rating) {
          SetRating(xiph_comment, song.rating());
        }
        if (save_cover) {
          SetEmbeddedArt(xiph_comment, cover.data, cover.mime_type);
        }
      }
    }
  }

  const bool success = fileref->save();
#ifdef Q_OS_LINUX
  if (success) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  return success ? Result(Result::ErrorCode::Success) : Result(Result::ErrorCode::FileSaveError);

}

void TagReaderTagLib::SaveID3v2Tag(TagLib::ID3v2::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  SetTextFrame("TPOS", song.disc() <= 0 ? QString() : QString::number(song.disc()), tag);
  SetTextFrame("TCOM", song.composer().empty() ? std::string() : song.composer(), tag);
  SetTextFrame("TIT1", song.grouping().empty() ? std::string() : song.grouping(), tag);
  SetTextFrame("TOPE", song.performer().empty() ? std::string() : song.performer(), tag);
  // Skip TPE1 (which is the artist) here because we already set it
  SetTextFrame("TPE2", song.albumartist().empty() ? std::string() : song.albumartist(), tag);
  SetTextFrame("TCMP", song.compilation() ? QString::number(1) : QString(), tag);
  SetUnsyncLyricsFrame(song.lyrics().empty() ? std::string() : song.lyrics(), tag);

}

void TagReaderTagLib::SaveAPETag(TagLib::APE::Tag *tag, const spb::tagreader::SongMetadata &song) const {

  tag->setItem("album artist", TagLib::APE::Item("album artist", TagLib::StringList(song.albumartist().c_str())));
  tag->addValue("disc", QStringToTagLibString(song.disc() <= 0 ? QString() : QString::number(song.disc())), true);
  tag->setItem("composer", TagLib::APE::Item("composer", TagLib::StringList(song.composer().c_str())));
  tag->setItem("grouping", TagLib::APE::Item("grouping", TagLib::StringList(song.grouping().c_str())));
  tag->setItem("performer", TagLib::APE::Item("performer", TagLib::StringList(song.performer().c_str())));
  tag->setItem("lyrics", TagLib::APE::Item("lyrics", TagLib::String(song.lyrics())));
  tag->addValue("compilation", QStringToTagLibString(song.compilation() ? QString::number(1) : QString()), true);

}

void TagReaderTagLib::SetTextFrame(const char *id, const QString &value, TagLib::ID3v2::Tag *tag) const {

  const QByteArray utf8(value.toUtf8());
  SetTextFrame(id, std::string(utf8.constData(), utf8.length()), tag);

}

void TagReaderTagLib::SetTextFrame(const char *id, const std::string &value, TagLib::ID3v2::Tag *tag) const {

  TagLib::ByteVector id_vector(id);
  QVector<TagLib::ByteVector> frames_buffer;

  // Store and clear existing frames
  while (tag->frameListMap().contains(id_vector) && tag->frameListMap()[id_vector].size() != 0) {
    frames_buffer.push_back(tag->frameListMap()[id_vector].front()->render());
    tag->removeFrame(tag->frameListMap()[id_vector].front());
  }

  if (value.empty()) return;

  // If no frames stored create empty frame
  if (frames_buffer.isEmpty()) {
    TagLib::ID3v2::TextIdentificationFrame frame(id_vector, TagLib::String::UTF8);
    frames_buffer.push_back(frame.render());
  }

  // Update and add the frames
  for (int i = 0; i < frames_buffer.size(); ++i) {
    TagLib::ID3v2::TextIdentificationFrame *frame = new TagLib::ID3v2::TextIdentificationFrame(frames_buffer.at(i));
    if (i == 0) {
      frame->setText(StdStringToTagLibString(value));
    }
    // add frame takes ownership and clears the memory
    tag->addFrame(frame);
  }

}

void TagReaderTagLib::SetUserTextFrame(const QString &description, const QString &value, TagLib::ID3v2::Tag *tag) const {

  const QByteArray descr_utf8(description.toUtf8());
  const QByteArray value_utf8(value.toUtf8());
  SetUserTextFrame(std::string(descr_utf8.constData(), descr_utf8.length()), std::string(value_utf8.constData(), value_utf8.length()), tag);

}

void TagReaderTagLib::SetUserTextFrame(const std::string &description, const std::string &value, TagLib::ID3v2::Tag *tag) const {

  const TagLib::String t_description = StdStringToTagLibString(description);
  TagLib::ID3v2::UserTextIdentificationFrame *frame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, t_description);
  if (frame) {
    tag->removeFrame(frame);
  }

  // Create and add a new frame
  frame = new TagLib::ID3v2::UserTextIdentificationFrame(TagLib::String::UTF8);
  frame->setDescription(t_description);
  frame->setText(StdStringToTagLibString(value));
  tag->addFrame(frame);

}

void TagReaderTagLib::SetUnsyncLyricsFrame(const std::string &value, TagLib::ID3v2::Tag *tag) const {

  TagLib::ByteVector id_vector("USLT");
  QVector<TagLib::ByteVector> frames_buffer;

  // Store and clear existing frames
  while (tag->frameListMap().contains(id_vector) && tag->frameListMap()[id_vector].size() != 0) {
    frames_buffer.push_back(tag->frameListMap()[id_vector].front()->render());
    tag->removeFrame(tag->frameListMap()[id_vector].front());
  }

  if (value.empty()) return;

  // If no frames stored create empty frame
  if (frames_buffer.isEmpty()) {
    TagLib::ID3v2::UnsynchronizedLyricsFrame frame(TagLib::String::UTF8);
    frame.setDescription("Clementine editor");
    frames_buffer.push_back(frame.render());
  }

  // Update and add the frames
  for (int i = 0; i < frames_buffer.size(); ++i) {
    TagLib::ID3v2::UnsynchronizedLyricsFrame *frame = new TagLib::ID3v2::UnsynchronizedLyricsFrame(frames_buffer.at(i));
    if (i == 0) {
      frame->setText(StdStringToTagLibString(value));
    }
    // add frame takes ownership and clears the memory
    tag->addFrame(frame);
  }

}

TagReaderBase::Result TagReaderTagLib::LoadEmbeddedArt(const QString &filename, QByteArray &data) const {

  if (filename.isEmpty()) {
    return Result::ErrorCode::FilenameMissing;
  }

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return Result::ErrorCode::FileDoesNotExist;
  }

  qLog(Debug) << "Loading art from" << filename;

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return Result::ErrorCode::FileOpenError;
  }

  // FLAC
  if (TagLib::FLAC::File *flac_file = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    if (flac_file->xiphComment()) {
      TagLib::List<TagLib::FLAC::Picture*> pictures = flac_file->pictureList();
      if (!pictures.isEmpty()) {
        for (TagLib::FLAC::Picture *picture : pictures) {
          if (picture->type() == TagLib::FLAC::Picture::FrontCover && picture->data().size() > 0) {
            data = QByteArray(picture->data().data(), picture->data().size());
            if (!data.isEmpty()) {
              return Result::ErrorCode::Success;
            }
          }
        }
      }
    }
  }

  // WavPack
  if (TagLib::WavPack::File *wavpack_file = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    if (wavpack_file->APETag()) {
      data = LoadEmbeddedAPEArt(wavpack_file->APETag()->itemListMap());
      if (!data.isEmpty()) {
        return Result::ErrorCode::Success;
      }
    }
  }

  // APE
  if (TagLib::APE::File *ape_file = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    if (ape_file->APETag()) {
      data = LoadEmbeddedAPEArt(ape_file->APETag()->itemListMap());
      if (!data.isEmpty()) {
        return Result::ErrorCode::Success;
      }
    }
  }

  // MPC
  if (TagLib::MPC::File *mpc_file = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    if (mpc_file->APETag()) {
      data = LoadEmbeddedAPEArt(mpc_file->APETag()->itemListMap());
      if (!data.isEmpty()) {
        return Result::ErrorCode::Success;
      }
    }
  }

  // Ogg Vorbis / Opus / Speex
  if (TagLib::Ogg::XiphComment *xiph_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    TagLib::Ogg::FieldListMap map = xiph_comment->fieldListMap();

    TagLib::List<TagLib::FLAC::Picture*> pictures = xiph_comment->pictureList();
    if (!pictures.isEmpty()) {
      for (TagLib::FLAC::Picture *picture : pictures) {
        if (picture->type() == TagLib::FLAC::Picture::FrontCover && picture->data().size() > 0) {
          data = QByteArray(picture->data().data(), picture->data().size());
          if (!data.isEmpty()) {
            return Result::ErrorCode::Success;
          }
        }
      }
    }

    // Ogg lacks a definitive standard for embedding cover art, but it seems b64 encoding a field called COVERART is the general convention
    if (map.contains("COVERART")) {
      data = QByteArray::fromBase64(map["COVERART"].toString().toCString());
      if (!data.isEmpty()) {
        return Result::ErrorCode::Success;
      }
    }

  }

  // MP3
  if (TagLib::MPEG::File *file_mp3 = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    if (file_mp3->ID3v2Tag()) {
      TagLib::ID3v2::FrameList apic_frames = file_mp3->ID3v2Tag()->frameListMap()["APIC"];
      if (apic_frames.isEmpty()) {
        return Result::ErrorCode::Success;
      }

      TagLib::ID3v2::AttachedPictureFrame *picture = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(apic_frames.front());

      data = QByteArray(reinterpret_cast<const char*>(picture->picture().data()), picture->picture().size());
      if (!data.isEmpty()) {
        return Result::ErrorCode::Success;
      }
    }
  }

  // MP4/AAC
  if (TagLib::MP4::File *aac_file = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = aac_file->tag();
    if (tag && tag->item("covr").isValid()) {
      const TagLib::MP4::CoverArtList &art_list = tag->item("covr").toCoverArtList();

      if (!art_list.isEmpty()) {
        // Just take the first one for now
        const TagLib::MP4::CoverArt &art = art_list.front();
        data = QByteArray(art.data().data(), art.data().size());
        if (!data.isEmpty()) {
          return Result::ErrorCode::Success;
        }
      }
    }
  }

  return Result::ErrorCode::Success;

}

QByteArray TagReaderTagLib::LoadEmbeddedAPEArt(const TagLib::APE::ItemListMap &map) const {

  TagLib::APE::ItemListMap::ConstIterator it = map.find("COVER ART (FRONT)");
  if (it != map.end()) {
    TagLib::ByteVector data = it->second.binaryData();

    int pos = data.find('\0') + 1;
    if ((pos > 0) && (static_cast<uint>(pos) < data.size())) {
      return QByteArray(data.data() + pos, data.size() - pos);
    }
  }

  return QByteArray();

}

void TagReaderTagLib::SetEmbeddedArt(TagLib::FLAC::File *flac_file, TagLib::Ogg::XiphComment *xiph_comment, const QByteArray &data, const QString &mime_type) const {

  (void)xiph_comment;

  flac_file->removePictures();

  if (!data.isEmpty()) {
    TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture();
    picture->setType(TagLib::FLAC::Picture::FrontCover);
    picture->setMimeType(QStringToTagLibString(mime_type));
    picture->setData(TagLib::ByteVector(data.constData(), data.size()));
    flac_file->addPicture(picture);
  }

}

void TagReaderTagLib::SetEmbeddedArt(TagLib::Ogg::XiphComment *xiph_comment, const QByteArray &data, const QString &mime_type) const {

  xiph_comment->removeAllPictures();

  if (!data.isEmpty()) {
    TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture();
    picture->setType(TagLib::FLAC::Picture::FrontCover);
    picture->setMimeType(QStringToTagLibString(mime_type));
    picture->setData(TagLib::ByteVector(data.constData(), data.size()));
    xiph_comment->addPicture(picture);
  }

}

void TagReaderTagLib::SetEmbeddedArt(TagLib::ID3v2::Tag *tag, const QByteArray &data, const QString &mime_type) const {

  // Remove existing covers
  TagLib::ID3v2::FrameList apiclist = tag->frameListMap()["APIC"];
  for (TagLib::ID3v2::FrameList::ConstIterator it = apiclist.begin(); it != apiclist.end(); ++it) {
    TagLib::ID3v2::AttachedPictureFrame *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(*it);
    tag->removeFrame(frame, false);
  }

  if (!data.isEmpty()) {
    // Add new cover
    TagLib::ID3v2::AttachedPictureFrame *frontcover = nullptr;
    frontcover = new TagLib::ID3v2::AttachedPictureFrame("APIC");
    frontcover->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
    frontcover->setMimeType(QStringToTagLibString(mime_type));
    frontcover->setPicture(TagLib::ByteVector(data.constData(), data.size()));
    tag->addFrame(frontcover);
  }

}

void TagReaderTagLib::SetEmbeddedArt(TagLib::MP4::File *aac_file, TagLib::MP4::Tag *tag, const QByteArray &data, const QString &mime_type) const {

  (void)aac_file;

  TagLib::MP4::CoverArtList covers;
  if (data.isEmpty()) {
    if (tag->contains("covr")) tag->removeItem("covr");
  }
  else {
    TagLib::MP4::CoverArt::Format cover_format = TagLib::MP4::CoverArt::Format::JPEG;
    if (mime_type == QLatin1String("image/jpeg")) {
      cover_format = TagLib::MP4::CoverArt::Format::JPEG;
    }
    else if (mime_type == QLatin1String("image/png")) {
      cover_format = TagLib::MP4::CoverArt::Format::PNG;
    }
    else {
      return;
    }
    covers.append(TagLib::MP4::CoverArt(cover_format, TagLib::ByteVector(data.constData(), data.size())));
    tag->setItem("covr", covers);
  }

}

TagReaderBase::Result TagReaderTagLib::SaveEmbeddedArt(const QString &filename, const spb::tagreader::SaveEmbeddedArtRequest &request) const {

  if (filename.isEmpty()) {
    return Result::ErrorCode::FilenameMissing;
  }

  qLog(Debug) << "Saving art to" << filename;

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return Result::ErrorCode::FileDoesNotExist;
  }

  const Cover cover = LoadCoverFromRequest(filename, request);

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return Result::ErrorCode::FileOpenError;
  }

  // FLAC
  if (TagLib::FLAC::File *flac_file = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    TagLib::Ogg::XiphComment *xiph_comment = flac_file->xiphComment(true);
    if (xiph_comment) {
      SetEmbeddedArt(flac_file, xiph_comment, cover.data, cover.mime_type);
    }
  }

  // Ogg Vorbis / Opus / Speex
  else if (TagLib::Ogg::XiphComment *xiph_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    SetEmbeddedArt(xiph_comment, cover.data, cover.mime_type);
  }

  // MP3
  else if (TagLib::MPEG::File *file_mp3 = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = file_mp3->ID3v2Tag();
    if (tag) {
      SetEmbeddedArt(tag, cover.data, cover.mime_type);
    }
  }

  // MP4/AAC
  else if (TagLib::MP4::File *aac_file = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = aac_file->tag();
    if (tag) {
      SetEmbeddedArt(aac_file, tag, cover.data, cover.mime_type);
    }
  }

  // Not supported.
  else {
    qLog(Error) << "Saving embedded art is not supported for %1" << filename;
    return Result::ErrorCode::Unsupported;
  }

  const bool success = fileref->file()->save();
#ifdef Q_OS_LINUX
  if (success) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  return success ? Result::ErrorCode::Success : Result::ErrorCode::FileSaveError;

}

TagLib::ID3v2::PopularimeterFrame *TagReaderTagLib::GetPOPMFrameFromTag(TagLib::ID3v2::Tag *tag) {

  TagLib::ID3v2::PopularimeterFrame *frame = nullptr;

  const TagLib::ID3v2::FrameListMap &map = tag->frameListMap();
  if (map.contains("POPM")) {
    frame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(map["POPM"].front());
  }

  if (!frame) {
    frame = new TagLib::ID3v2::PopularimeterFrame();
    tag->addFrame(frame);
  }

  return frame;

}

void TagReaderTagLib::SetPlaycount(TagLib::Ogg::XiphComment *xiph_comment, const uint playcount) const {

  if (playcount > 0) {
    xiph_comment->addField("FMPS_PLAYCOUNT", TagLib::String::number(static_cast<int>(playcount)), true);
  }
  else {
    xiph_comment->removeFields("FMPS_PLAYCOUNT");
  }

}

void TagReaderTagLib::SetPlaycount(TagLib::APE::Tag *tag, const uint playcount) const {

  if (playcount > 0) {
    tag->setItem("FMPS_Playcount", TagLib::APE::Item("FMPS_Playcount", TagLib::String::number(static_cast<int>(playcount))));
  }
  else {
    tag->removeItem("FMPS_Playcount");
  }

}

void TagReaderTagLib::SetPlaycount(TagLib::ID3v2::Tag *tag, const uint playcount) const {

  SetUserTextFrame(QStringLiteral("FMPS_Playcount"), QString::number(playcount), tag);
  TagLib::ID3v2::PopularimeterFrame *frame = GetPOPMFrameFromTag(tag);
  if (frame) {
    frame->setCounter(playcount);
  }

}

void TagReaderTagLib::SetPlaycount(TagLib::MP4::Tag *tag, const uint playcount) const {

  if (playcount > 0) {
    tag->setItem(kMP4_FMPS_Playcount_ID, TagLib::MP4::Item(TagLib::String::number(static_cast<int>(playcount))));
  }
  else {
    tag->removeItem(kMP4_FMPS_Playcount_ID);
  }

}

void TagReaderTagLib::SetPlaycount(TagLib::ASF::Tag *tag, const uint playcount) const {

  if (playcount > 0) {
    tag->setAttribute("FMPS/Playcount", TagLib::ASF::Attribute(QStringToTagLibString(QString::number(playcount))));
  }
  else {
    tag->removeItem("FMPS/Playcount");
  }

}

TagReaderBase::Result TagReaderTagLib::SaveSongPlaycountToFile(const QString &filename, const uint playcount) const {

  if (filename.isEmpty()) {
    return Result::ErrorCode::FilenameMissing;
  }

  qLog(Debug) << "Saving song playcount to" << filename;

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return Result::ErrorCode::FileDoesNotExist;
  }

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return Result::ErrorCode::FileOpenError;
  }

  if (TagLib::FLAC::File *flac_file = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    TagLib::Ogg::XiphComment *xiph_comment = flac_file->xiphComment(true);
    if (xiph_comment) {
      SetPlaycount(xiph_comment, playcount);
    }
  }
  else if (TagLib::WavPack::File *wavpack_file = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = wavpack_file->APETag(true);
    if (tag) {
      SetPlaycount(tag, playcount);
    }
  }
  else if (TagLib::APE::File *ape_file = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = ape_file->APETag(true);
    if (tag) {
      SetPlaycount(tag, playcount);
    }
  }
  else if (TagLib::Ogg::XiphComment *xiph_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    if (xiph_comment) {
      SetPlaycount(xiph_comment, playcount);
    }
  }
  else if (TagLib::MPEG::File *mpeg_file = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = mpeg_file->ID3v2Tag(true);
    if (tag) {
      SetPlaycount(tag, playcount);
    }
  }
  else if (TagLib::MP4::File *mp4_file = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = mp4_file->tag();
    if (tag) {
      SetPlaycount(tag, playcount);
    }
  }
  else if (TagLib::MPC::File *mpc_file = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = mpc_file->APETag(true);
    if (tag) {
      SetPlaycount(tag, playcount);
    }
  }
  else if (TagLib::ASF::File *asf_file = dynamic_cast<TagLib::ASF::File*>(fileref->file())) {
    TagLib::ASF::Tag *tag = asf_file->tag();
    if (tag && playcount > 0) {
      tag->addAttribute("FMPS/Playcount", TagLib::ASF::Attribute(QStringToTagLibString(QString::number(playcount))));
    }
  }
  else {
    return Result::ErrorCode::Unsupported;
  }

  const bool success = fileref->save();
#ifdef Q_OS_LINUX
  if (success) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  return success ? Result::ErrorCode::Success : Result::ErrorCode::FileSaveError;

}

void TagReaderTagLib::SetRating(TagLib::Ogg::XiphComment *xiph_comment, const float rating) const {

  if (rating > 0.0F) {
    xiph_comment->addField("FMPS_RATING", QStringToTagLibString(QString::number(rating)), true);
  }
  else {
    xiph_comment->removeFields("FMPS_RATING");
  }

}

void TagReaderTagLib::SetRating(TagLib::APE::Tag *tag, const float rating) const {

  if (rating > 0.0F) {
    tag->setItem("FMPS_Rating", TagLib::APE::Item("FMPS_Rating", TagLib::StringList(QStringToTagLibString(QString::number(rating)))));
  }
  else {
    tag->removeItem("FMPS_Rating");
  }

}

void TagReaderTagLib::SetRating(TagLib::ID3v2::Tag *tag, const float rating) const {

  SetUserTextFrame(QStringLiteral("FMPS_Rating"), QString::number(rating), tag);
  TagLib::ID3v2::PopularimeterFrame *frame = GetPOPMFrameFromTag(tag);
  if (frame) {
    frame->setRating(ConvertToPOPMRating(rating));
  }

}

void TagReaderTagLib::SetRating(TagLib::MP4::Tag *tag, const float rating) const {

  tag->setItem(kMP4_FMPS_Rating_ID, TagLib::StringList(QStringToTagLibString(QString::number(rating))));

}

void TagReaderTagLib::SetRating(TagLib::ASF::Tag *tag, const float rating) const {

  tag->addAttribute("FMPS/Rating", TagLib::ASF::Attribute(QStringToTagLibString(QString::number(rating))));

}

TagReaderBase::Result TagReaderTagLib::SaveSongRatingToFile(const QString &filename, const float rating) const {

  if (filename.isEmpty()) {
    return Result::ErrorCode::FilenameMissing;
  }

  qLog(Debug) << "Saving song rating to" << filename;

  if (!QFile::exists(filename)) {
    qLog(Error) << "File" << filename << "does not exist";
    return Result::ErrorCode::FileDoesNotExist;
  }

  if (rating < 0) {
    return Result::ErrorCode::Success;
  }

  std::unique_ptr<TagLib::FileRef> fileref(factory_->GetFileRef(filename));
  if (!fileref || fileref->isNull()) {
    qLog(Error) << "TagLib could not open file" << filename;
    return Result::ErrorCode::FileOpenError;
  }

  if (TagLib::FLAC::File *flac_file = dynamic_cast<TagLib::FLAC::File*>(fileref->file())) {
    TagLib::Ogg::XiphComment *xiph_comment = flac_file->xiphComment(true);
    if (xiph_comment) {
      SetRating(xiph_comment, rating);
    }
  }
  else if (TagLib::WavPack::File *wavpack_file = dynamic_cast<TagLib::WavPack::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = wavpack_file->APETag(true);
    if (tag) {
      SetRating(tag, rating);
    }
  }
  else if (TagLib::APE::File *ape_file = dynamic_cast<TagLib::APE::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = ape_file->APETag(true);
    if (tag) {
      SetRating(tag, rating);
    }
  }
  else if (TagLib::Ogg::XiphComment *xiph_comment = dynamic_cast<TagLib::Ogg::XiphComment*>(fileref->file()->tag())) {
    SetRating(xiph_comment, rating);
  }
  else if (TagLib::MPEG::File *mpeg_file = dynamic_cast<TagLib::MPEG::File*>(fileref->file())) {
    TagLib::ID3v2::Tag *tag = mpeg_file->ID3v2Tag(true);
    if (tag) {
      SetRating(tag, rating);
    }
  }
  else if (TagLib::MP4::File *mp4_file = dynamic_cast<TagLib::MP4::File*>(fileref->file())) {
    TagLib::MP4::Tag *tag = mp4_file->tag();
    if (tag) {
      SetRating(tag, rating);
    }
  }
  else if (TagLib::ASF::File *asf_file = dynamic_cast<TagLib::ASF::File*>(fileref->file())) {
    TagLib::ASF::Tag *tag = asf_file->tag();
    if (tag) {
      SetRating(tag, rating);
    }
  }
  else if (TagLib::MPC::File *mpc_file = dynamic_cast<TagLib::MPC::File*>(fileref->file())) {
    TagLib::APE::Tag *tag = mpc_file->APETag(true);
    if (tag) {
      SetRating(tag, rating);
    }
  }
  else {
    qLog(Error) << "Unsupported file for saving rating for" << filename;
    return Result::ErrorCode::Unsupported;
  }

  const bool success = fileref->save();
#ifdef Q_OS_LINUX
  if (success) {
    // Linux: inotify doesn't seem to notice the change to the file unless we change the timestamps as well. (this is what touch does)
    utimensat(0, QFile::encodeName(filename).constData(), nullptr, 0);
  }
#endif  // Q_OS_LINUX

  if (!success) {
    qLog(Error) << "TagLib hasn't been able to save file" << filename;
  }

  return success ? Result::ErrorCode::Success : Result::ErrorCode::FileSaveError;

}
