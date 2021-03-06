﻿#ifndef PLATFORM_FS_HPP_1094D8B6_7681_46C8_9C08_C5253376E988
#define PLATFORM_FS_HPP_1094D8B6_7681_46C8_9C08_C5253376E988
#pragma once

/*
platform.fs.hpp

*/
/*
Copyright © 2017 Far Group
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

namespace os::fs
{
	namespace detail
	{
		struct find_handle_closer
		{
			void operator()(HANDLE Handle) const;
		};

		struct find_file_handle_closer
		{
			void operator()(HANDLE Handle) const;
		};

		struct find_volume_handle_closer
		{
			void operator()(HANDLE Handle) const;
		};

		struct find_notification_handle_closer
		{
			void operator()(HANDLE Handle) const;
		};
	}

	using find_handle = os::detail::handle_t<detail::find_handle_closer>;
	using find_file_handle = os::detail::handle_t<detail::find_file_handle_closer>;
	using find_volume_handle = os::detail::handle_t<detail::find_volume_handle_closer>;
	using find_notification_handle = os::detail::handle_t<detail::find_notification_handle_closer>;

	using drives_set = std::bitset<26>;

	using security_descriptor = block_ptr<SECURITY_DESCRIPTOR, default_buffer_size>;

	struct find_data
	{
		time_point CreationTime;
		time_point LastAccessTime;
		time_point LastWriteTime;
		time_point ChangeTime;
		unsigned long long nFileSize{};
		unsigned long long nAllocationSize{};
		unsigned long long FileId{};
		string strFileName;
		string strAlternateFileName;
		DWORD dwFileAttributes{};
		DWORD dwReserved0{};
	};

	bool is_standard_drive_letter(wchar_t Letter);
	int get_drive_number(wchar_t Letter);
	string get_drive(wchar_t Letter);
	string get_root_directory(wchar_t Letter);

	class enum_drives: public enumerator<enum_drives, wchar_t>
	{
		IMPLEMENTS_ENUMERATOR(enum_drives);

	public:
		explicit enum_drives(drives_set Drives);

	private:
		bool get(size_t Index, wchar_t& Value) const;

		drives_set m_Drives;
		mutable size_t m_CurrentIndex{};
	};

	class enum_files: public enumerator<enum_files, find_data>
	{
		IMPLEMENTS_ENUMERATOR(enum_files);

	public:
		explicit enum_files(const string_view& Object, bool ScanSymLink = true);

	private:
		bool get(size_t Index, find_data& Value) const;

		string m_Object;
		bool m_ScanSymlink;
		mutable find_file_handle m_Handle;
	};

	class enum_names: public enumerator<enum_names, string>
	{
		IMPLEMENTS_ENUMERATOR(enum_names);

	public:
		explicit enum_names(const string_view& Object);

	private:
		bool get(size_t Index, string& Value) const;

		string m_Object;
		mutable find_handle m_Handle;
	};

	class enum_streams: public enumerator<enum_streams, WIN32_FIND_STREAM_DATA>
	{
		IMPLEMENTS_ENUMERATOR(enum_streams);

	public:
		explicit enum_streams(const string_view& Object);

	private:
		bool get(size_t Index, WIN32_FIND_STREAM_DATA& Value) const;

		string m_Object;
		mutable find_file_handle m_Handle;
	};

	class enum_volumes: public enumerator<enum_volumes, string>
	{
		IMPLEMENTS_ENUMERATOR(enum_volumes);

	public:
		enum_volumes();

	private:
		bool get(size_t Index, string& Value) const;

		string m_Object;
		mutable find_volume_handle m_Handle;
	};

	class file: public conditional<file>
	{
	public:
		NONCOPYABLE(file);
		MOVABLE(file);

		file():
			m_Pointer(),
			m_NeedSyncPointer(),
			m_ShareMode()
		{
		}

		template<typename... args>
		explicit file(args... Args)
		{
			Open(FWD(Args)...);
		}

		bool operator!() const noexcept;
		// TODO: half of these should be free functions
		bool Open(const string_view& Object, DWORD DesiredAccess, DWORD ShareMode, SECURITY_ATTRIBUTES* SecurityAttributes, DWORD CreationDistribution, DWORD FlagsAndAttributes = 0, const file* TemplateFile = nullptr, bool ForceElevation = false);
		// TODO: async overloads when needed
		bool Read(void* Buffer, size_t NumberOfBytesToRead, size_t& NumberOfBytesRead) const;
		bool Write(const void* Buffer, size_t NumberOfBytesToWrite) const;
		unsigned long long GetPointer() const;
		bool SetPointer(long long DistanceToMove, unsigned long long* NewFilePointer, DWORD MoveMethod) const;
		bool SetEnd();
		bool GetTime(time_point* CreationTime, time_point* LastAccessTime, time_point* LastWriteTime, time_point* ChangeTime) const;
		bool SetTime(const time_point* CreationTime, const time_point* LastAccessTime, const time_point* LastWriteTime, const time_point* ChangeTime) const;
		bool GetSize(unsigned long long& Size) const;
		bool FlushBuffers() const;
		bool GetInformation(BY_HANDLE_FILE_INFORMATION& info) const;
		bool IoControl(DWORD IoControlCode, void* InBuffer, DWORD InBufferSize, void* OutBuffer, DWORD OutBufferSize, DWORD* BytesReturned, OVERLAPPED* Overlapped = nullptr) const;
		bool GetStorageDependencyInformation(GET_STORAGE_DEPENDENCY_FLAG Flags, ULONG StorageDependencyInfoSize, PSTORAGE_DEPENDENCY_INFO StorageDependencyInfo, PULONG SizeUsed) const;
		bool NtQueryDirectoryFile(void* FileInformation, size_t Length, FILE_INFORMATION_CLASS FileInformationClass, bool ReturnSingleEntry, const wchar_t* FileName, bool RestartScan, NTSTATUS* Status = nullptr) const;
		bool NtQueryInformationFile(void* FileInformation, size_t Length, FILE_INFORMATION_CLASS FileInformationClass, NTSTATUS* Status = nullptr) const;
		bool GetFinalPathName(string& FinalFilePath) const;
		void Close();
		bool Eof() const;
		const string& GetName() const;
		const handle& get() const;

	private:
		void SyncPointer() const;

		handle m_Handle;
		mutable unsigned long long m_Pointer;
		mutable bool m_NeedSyncPointer;
		string m_Name;
		DWORD m_ShareMode;
	};

	class file_walker: public file
	{
	public:
		file_walker();
		~file_walker();

		bool InitWalk(size_t BlockSize);
		bool Step();
		unsigned long long GetChunkOffset() const;
		DWORD GetChunkSize() const;
		int GetPercent() const;

	private:
		struct Chunk;
		std::vector<Chunk> m_ChunkList;
		unsigned long long m_FileSize{};
		unsigned long long m_AllocSize{};
		unsigned long long m_ProcessedSize{};
		std::vector<Chunk>::iterator m_CurrentChunk{};
		DWORD m_ChunkSize{};
		bool m_IsSparse{};
	};

	class file_status
	{
	public:
		file_status();
		explicit file_status(const string_view& Object);

		bool check(DWORD Data) const;

	private:
		DWORD m_Data;
	};

	bool exists(file_status Status);
	bool exists(const string_view& Object);

	bool is_file(file_status Status);
	bool is_file(const string_view& Object);

	bool is_directory(file_status Status);
	bool is_directory(const string_view& Object);

	bool is_not_empty_directory(const string& Object);

	class process_current_directory_guard: noncopyable
	{
	public:
		process_current_directory_guard(bool Active, const std::function<string()>& Provider);
		~process_current_directory_guard();

	private:
		string m_Directory;
		bool m_Active;
	};

	namespace low
	{
		HANDLE create_file(const wchar_t* FileName, DWORD DesiredAccess, DWORD ShareMode, SECURITY_ATTRIBUTES* SecurityAttributes, DWORD CreationDistribution, DWORD FlagsAndAttributes, HANDLE TemplateFile);
		bool create_directory(const wchar_t* TemplateDirectory, const wchar_t* NewDirectory, SECURITY_ATTRIBUTES* SecurityAttributes);
		bool remove_directory(const wchar_t* PathName);
		bool delete_file(const wchar_t* FileName);
		DWORD get_file_attributes(const wchar_t* FileName);
		bool set_file_attributes(const wchar_t* FileName, DWORD Attributes);
		bool create_hard_link(const wchar_t* FileName, const wchar_t* ExistingFileName, SECURITY_ATTRIBUTES* SecurityAttributes);
		bool copy_file(const wchar_t* ExistingFileName, const wchar_t* NewFileName, LPPROGRESS_ROUTINE ProgressRoutine, void* Data, BOOL* Cancel, DWORD CopyFlags);
		bool move_file(const wchar_t* ExistingFileName, const wchar_t* NewFileName, DWORD Flags);
		bool detach_virtual_disk(const wchar_t* Object, VIRTUAL_STORAGE_TYPE& VirtualStorageType);
		bool get_disk_free_space(const wchar_t* DirectoryName, unsigned long long* FreeBytesAvailableToCaller, unsigned long long* TotalNumberOfBytes, unsigned long long* TotalNumberOfFreeBytes);
		bool set_file_encryption(const wchar_t* FileName, bool Encrypt);
	}

	bool GetProcessRealCurrentDirectory(string& Directory);
	bool SetProcessRealCurrentDirectory(const string& Directory);
	void InitCurrentDirectory();
	string GetCurrentDirectory();
	bool SetCurrentDirectory(const string& PathName, bool Validate = true);

	bool create_directory(const string_view& PathName, SECURITY_ATTRIBUTES* SecurityAttributes = nullptr);
	bool create_directory(const string_view& TemplateDirectory, const string_view& NewDirectory, SECURITY_ATTRIBUTES* SecurityAttributes = nullptr);
	bool remove_directory(const string_view& DirName);
	handle create_file(const string_view& Object, DWORD DesiredAccess, DWORD ShareMode, SECURITY_ATTRIBUTES* SecurityAttributes, DWORD CreationDistribution, DWORD FlagsAndAttributes = 0, HANDLE TemplateFile = nullptr, bool ForceElevation = false);
	bool delete_file(const string_view& FileName);
	bool copy_file(const string_view& ExistingFileName, const string_view& NewFileName, LPPROGRESS_ROUTINE ProgressRoutine, void* Data, BOOL* Cancel, DWORD CopyFlags);
	bool move_file(const string_view& ExistingFileName, const string_view& NewFileName, DWORD Flags = 0);
	DWORD get_file_attributes(const string_view& FileName);
	bool set_file_attributes(const string_view& FileName, DWORD FileAttributes);

	bool GetLongPathName(const string& ShortPath, string& LongPath);
	bool GetShortPathName(const string& LongPath, string& ShortPath);
	bool GetVolumeInformation(const string& RootPathName, string *pVolumeName, DWORD* VolumeSerialNumber, DWORD* MaximumComponentLength, DWORD* FileSystemFlags, string* FileSystemName);
	bool GetVolumeNameForVolumeMountPoint(const string& VolumeMountPoint, string& VolumeName);
	bool GetVolumePathNamesForVolumeName(const string& VolumeName, string& VolumePathNames);
	bool QueryDosDevice(const string& DeviceName, string& Path);
	bool SearchPath(const wchar_t* Path, const string& FileName, const wchar_t* Extension, string& strDest);
	bool GetTempPath(string& strBuffer);
	bool GetModuleFileName(HANDLE hProcess, HMODULE hModule, string& strFileName);
	security_descriptor get_file_security(const string_view& Object, SECURITY_INFORMATION RequestedInformation);
	bool set_file_security(const string_view& Object, SECURITY_INFORMATION RequestedInformation, const security_descriptor& SecurityDescriptor);

	bool get_disk_size(const string_view& Path, unsigned long long* TotalSize, unsigned long long* TotalFree, unsigned long long* UserFree);

	bool GetFileTimeSimple(const string& FileName, time_point* CreationTime, time_point* LastAccessTime, time_point* LastWriteTime, time_point* ChangeTime);

	bool get_find_data(const string& FileName, find_data& FindData, bool ScanSymLink = true);

	find_notification_handle FindFirstChangeNotification(const string& PathName, bool WatchSubtree, DWORD NotifyFilter);
	int GetFileTypeByName(const string& Name);
	bool IsDiskInDrive(const string& Root);

	bool create_hard_link(const string& FileName, const string& ExistingFileName, SECURITY_ATTRIBUTES* SecurityAttributes);

	bool CreateSymbolicLink(const string& SymlinkFileName, const string& TargetFileName, DWORD Flags);

	bool set_file_encryption(const string_view& FileName, bool Encrypt);
	bool detach_virtual_disk(const string_view& Object, VIRTUAL_STORAGE_TYPE& VirtualStorageType);



	bool CreateSymbolicLinkInternal(const string& Object, const string& Target, DWORD Flags);
}

#endif // PLATFORM_FS_HPP_1094D8B6_7681_46C8_9C08_C5253376E988
