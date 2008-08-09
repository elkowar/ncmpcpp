/***************************************************************************
 *   Copyright (C) 2008 by Andrzej Rybczak   *
 *   electricityispower@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "status_checker.h"
#include "helpers.h"
#include "settings.h"

#define FOR_EACH_MPD_DATA(x) for (; (x); (x) = mpd_data_get_next(x))

extern MpdObj *conn;
extern MpdData *playlist;

extern ncmpcpp_config Config;

extern Menu *mPlaylist;
extern Menu *mBrowser;
extern Menu *wCurrent;
extern Menu *mSearcher;
extern Menu *mLibArtists;
extern Menu *mLibAlbums;
extern Menu *mLibSongs;
extern Window *wHeader;
extern Window *wFooter;

extern vector<Song> vPlaylist;
extern vector<Song> vSearched;
extern vector<MpdDataType> vFileType;
extern vector<string> vNameList;
extern vector<long long> vHashList;

extern time_t block_delay;
extern time_t timer;
extern time_t now;

extern int now_playing;
extern int playing_song_scroll_begin;

extern int block_statusbar_update_delay;

extern string browsed_dir;

extern string player_state;
extern string volume_state;
extern string switch_state;

extern string mpd_repeat;
extern string mpd_random;
extern string mpd_crossfade;
extern string mpd_db_updating;

extern CurrScreen current_screen;

extern bool header_update_status;

extern bool allow_statusbar_unblock;
extern bool block_progressbar_update;
extern bool block_statusbar_update;
extern bool block_playlist_update;
extern bool block_library_update;

int old_playing;

void TraceMpdStatus()
{
	mpd_status_update(conn);
	now = time(NULL);
	
	if (now == timer+Config.playlist_disable_highlight_delay && current_screen == csPlaylist)
	{
		mPlaylist->Highlighting(!Config.playlist_disable_highlight_delay);
		mPlaylist->Refresh();
	}
	
	if (block_statusbar_update_delay > 0)
	{
		if (now >= block_delay+block_statusbar_update_delay)
			block_statusbar_update_delay = 0;
	}
	
	if (!block_statusbar_update_delay)
	{
		block_statusbar_update_delay = -1;
		
		if (Config.statusbar_visibility)
			block_statusbar_update = !allow_statusbar_unblock;
		else
			block_progressbar_update = !allow_statusbar_unblock;
		
		switch (mpd_player_get_state(conn))
		{
			case MPD_PLAYER_STOP:
				NcmpcppStatusChanged(conn, MPD_CST_STATE);
				break;
			case MPD_PLAYER_PLAY: case MPD_PLAYER_PAUSE:
				NcmpcppStatusChanged(conn, MPD_CST_ELAPSED_TIME); // restore status
				break;
		}
	}
	//wHeader->WriteXY(0,1, IntoStr(now_playing), 1);
}

void NcmpcppErrorCallback(MpdObj *conn, int errorid, char *msg, void *userdata)
{
	ShowMessage(msg);
}

void NcmpcppStatusChanged(MpdObj *conn, ChangedStatusType what)
{
	int sx, sy;
	wFooter->DisableBB();
	wFooter->AutoRefresh(0);
	wFooter->Bold(1);
	wFooter->GetXY(sx, sy);
	
	if (now_playing != mpd_player_get_current_song_pos(conn))
	{
		old_playing = now_playing;
		now_playing = mpd_player_get_current_song_pos(conn);
	}
	
	if (what & MPD_CST_PLAYLIST)
	{
		if (!block_playlist_update)
		{
			int playlist_length = mpd_playlist_get_playlist_length(conn);
			
			if (playlist_length != vPlaylist.size())
			{
				if (playlist_length < vPlaylist.size())
				{
					mPlaylist->Clear(!playlist_length && current_screen != csLibrary);
					vPlaylist.clear();
				}
				
				playlist = mpd_playlist_get_changes(conn, -1);
				
				vPlaylist.reserve(playlist_length);
				
				FOR_EACH_MPD_DATA(playlist)
				{
					if ((playlist_length > vPlaylist.size() && playlist->song->pos >= vPlaylist.size()) || playlist_length < vPlaylist.size())
					{
						vPlaylist.push_back(playlist->song);
						Song &s = vPlaylist.back();
						if (now_playing != s.GetPosition())
							mPlaylist->AddOption(DisplaySong(s));
						else
							mPlaylist->AddBoldOption(DisplaySong(s));
					}
				}
				mpd_data_free(playlist);
				
				if (current_screen == csPlaylist)
				{
					if (!playlist_length || mPlaylist->MaxChoice() < mPlaylist->GetHeight())
						mPlaylist->Hide();
					mPlaylist->Refresh();
				}
			}
			else
			{
				int i = 1;
				
				mPlaylist->BoldOption(old_playing+1, 0);
				mPlaylist->BoldOption(now_playing+1, 1);
				
				playlist = mpd_playlist_get_changes(conn, -1);
				
				for (vector<Song>::iterator it = vPlaylist.begin(); it != vPlaylist.end(); it++, i++)
				{
					Song ss = playlist->song;
					ss.GetEmptyFields(1);
					if (it->GetFile() != ss.GetFile() || it->GetTitle() != ss.GetTitle() || it->GetArtist() != ss.GetArtist() || it->GetAlbum() != ss.GetAlbum() || it->GetYear() != ss.GetYear() || it->GetTrack() != ss.GetTrack() || it->GetGenre() != ss.GetGenre() || it->GetComment() != ss.GetComment() || it->GetID() != ss.GetID() || it->GetPosition() != ss.GetPosition())
					{
						ss.GetEmptyFields(0);
						*it = ss;
						mPlaylist->UpdateOption(i, DisplaySong(ss));
					}
					playlist = mpd_data_get_next(playlist);
				}
				mpd_data_free(playlist);
			}
		}
		
		if (vPlaylist.empty())
			ShowMessage("Cleared playlist!");
		
		if (!vNameList.empty())
		{
			bool bold = 0;
			for (int i = 0; i < vFileType.size(); i++)
			{
				if (vFileType[i] == MPD_DATA_TYPE_SONG)
				{
					for (vector<Song>::const_iterator it = vPlaylist.begin(); it != vPlaylist.end(); it++)
					{
						if (it->GetHash() == vHashList[i])
						{
							bold = 1;
							break;
						}
					}
					mBrowser->BoldOption(i+1, bold);
					bold = 0;
				}
			}
		}
		if (!vSearched.empty())
		{
			bool bold = 0;
			int i = search_engine_static_option;
			for (vector<Song>::const_iterator it = vSearched.begin(); it != vSearched.end(); it++, i++)
			{
				for (vector<Song>::const_iterator j = vPlaylist.begin(); j != vPlaylist.end(); j++)
				{
					if (j->GetHash() == it->GetHash())
					{
						bold = 1;
						break;
					}
				}
				mSearcher->BoldOption(i+1, bold);
				bold = 0;
			}
		}
		block_library_update = 0;
	}
	if(what & MPD_CST_DATABASE)
	{
		GetDirectory(browsed_dir);
		block_library_update = 0;
	}
	if (what & MPD_CST_STATE)
	{
		int mpd_state = mpd_player_get_state(conn);
		switch (mpd_state)
		{
			case MPD_PLAYER_PLAY:
			{
				Song &s = vPlaylist[now_playing];
				player_state = "Playing: ";
				mPlaylist->BoldOption(now_playing+1, 1);
				break;
			}
			case MPD_PLAYER_PAUSE:
			{
				player_state = "[Paused] ";
				break;
			}
			case MPD_PLAYER_STOP:
			{
				WindowTitle("ncmpc++ ver. "VERSION);
				wFooter->SetColor(Config.progressbar_color);
				mvwhline(wFooter->RawWin(), 0, 0, 0, wFooter->GetWidth());
				wFooter->SetColor(Config.statusbar_color);
				mPlaylist->BoldOption(old_playing+1, 0);
				now_playing = -1;
				player_state.clear();
				break;
			}
		}
		if (!block_statusbar_update && Config.statusbar_visibility)
			wFooter->WriteXY(0, 1, player_state, player_state.empty());
	}
	if ((what & MPD_CST_ELAPSED_TIME))
	{
		Song &s = vPlaylist[now_playing];
		if (!player_state.empty())
		{
			WindowTitle(DisplaySong(s, Config.song_window_title_format));
			
			int elapsed = mpd_status_get_elapsed_song_time(conn);
			
			if (!block_statusbar_update && Config.statusbar_visibility)
			{
				string tracklength;
				if (s.GetTotalLength() > 0)
					tracklength = " [" + ShowTime(elapsed) + "/" + s.GetLength() + "]";
				else
					tracklength = " [" + ShowTime(elapsed) + "]";
				ncmpcpp_string_t playing_song = NCMPCPP_TO_WSTRING(OmitBBCodes(DisplaySong(s, Config.song_status_format)));
				
				int max_length_without_scroll = wFooter->GetWidth()-player_state.length()-tracklength.length();
				
				wFooter->WriteXY(0, 1, player_state);
				wFooter->Bold(0);
				if (playing_song.length() > max_length_without_scroll)
				{
#					ifdef UTF8_ENABLED
					playing_song += L" ** ";
#					else
					playing_song += " ** ";
#					endif
					const int scrollsize = max_length_without_scroll+playing_song.length();
					ncmpcpp_string_t part = playing_song.substr(playing_song_scroll_begin++, scrollsize);
					if (part.length() < scrollsize)
						part += playing_song.substr(0, scrollsize-part.length());
					wFooter->WriteXY(player_state.length(), 1, part);
					if (playing_song_scroll_begin >= playing_song.length())
						playing_song_scroll_begin = 0;
				}
				else
					wFooter->WriteXY(player_state.length(), 1, OmitBBCodes(DisplaySong(s, Config.song_status_format)), 1);
				wFooter->Bold(1);
				
				wFooter->WriteXY(wFooter->GetWidth()-tracklength.length(), 1, tracklength);
			}
			if (!block_progressbar_update)
			{
				double progressbar_size = (double)elapsed/(s.GetMinutesLength()*60+s.GetSecondsLength());
				int howlong = wFooter->GetWidth()*progressbar_size;
				wFooter->SetColor(Config.progressbar_color);
				mvwhline(wFooter->RawWin(), 0, 0, 0, wFooter->GetWidth());
				mvwhline(wFooter->RawWin(), 0, 0, '=',howlong);
				mvwaddch(wFooter->RawWin(), 0, howlong, '>');
				wFooter->SetColor(Config.statusbar_color);
			}
		}
		else
		{
			if (!block_statusbar_update && Config.statusbar_visibility)
				wFooter->WriteXY(0, 1, "", 1);
		}
	}
	if (what & MPD_CST_REPEAT)
	{
		mpd_repeat = (mpd_player_get_repeat(conn) ? "r" : "");
		ShowMessage("Repeat is " + (string)(mpd_repeat.empty() ? "off" : "on"));
		header_update_status = 1;

	}
	if (what & MPD_CST_RANDOM)
	{
		mpd_random = mpd_player_get_random(conn) ? "z" : "";
		ShowMessage("Random is " + (string)(mpd_random.empty() ? "off" : "on"));
		header_update_status = 1;
	}
	if (what & MPD_CST_CROSSFADE)
	{
		int crossfade = mpd_status_get_crossfade(conn);
		mpd_crossfade = crossfade ? "x" : "";
		ShowMessage("Crossfade set to " + IntoStr(crossfade) + " seconds");
		header_update_status = 1;
	}
	if (what & MPD_CST_UPDATING)
	{
		mpd_db_updating = mpd_status_db_is_updating(conn) ? "U" : "";
		ShowMessage(mpd_db_updating.empty() ? "Database update finished!" : "Database update started!");
		header_update_status = 1;
	}
	if (header_update_status && Config.header_visibility)
	{
		switch_state = mpd_repeat + mpd_random + mpd_crossfade + mpd_db_updating;
		
		wHeader->DisableBB();
		wHeader->Bold(1);
		wHeader->SetColor(Config.state_line_color);
		mvwhline(wHeader->RawWin(), 1, 0, 0, wHeader->GetWidth());
		if (!switch_state.empty())
		{
			wHeader->WriteXY(wHeader->GetWidth()-switch_state.length()-3, 1, "[");
			wHeader->SetColor(Config.state_flags_color);
			wHeader->WriteXY(wHeader->GetWidth()-switch_state.length()-2, 1, switch_state);
			wHeader->SetColor(Config.state_line_color);
			wHeader->WriteXY(wHeader->GetWidth()-2, 1, "]");
		}
		wHeader->Refresh();
		wHeader->SetColor(Config.header_color);
		wHeader->Bold(0);
		wHeader->EnableBB();
		header_update_status = 0;
	}
	if (what & MPD_CST_SONGID)
	{
		int id = mpd_player_get_current_song_pos(conn);
		if (!vPlaylist.empty() && id >= 0)
		{
			Song &s = vPlaylist[id];
			if (!mPlaylist->Empty())
			{
				if (old_playing >= 0)
					mPlaylist->BoldOption(old_playing+1, 0);
				mPlaylist->BoldOption(now_playing+1, 1);
			}
			if (!mpd_status_get_elapsed_song_time(conn))
				mvwhline(wFooter->RawWin(), 0, 0, 0, wFooter->GetWidth());
		}
		playing_song_scroll_begin = 0;
	}
	if ((what & MPD_CST_VOLUME) && Config.header_visibility)
	{
		int vol = mpd_status_get_volume(conn);
		volume_state = " Volume: " + IntoStr(vol) + "%";
		wHeader->SetColor(Config.volume_color);
		wHeader->WriteXY(wHeader->GetWidth()-volume_state.length(), 0, volume_state);
		wHeader->SetColor(Config.header_color);
	}
	wFooter->Bold(0);
	wFooter->GotoXY(sx, sy);
	wFooter->Refresh();
	wFooter->AutoRefresh(1);
	wFooter->EnableBB();
}

