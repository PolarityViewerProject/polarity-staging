/** 
 * @file llfile.h
 * @author Michael Schlachter
 * @date 2006-03-23
 * @brief Declaration of cross-platform POSIX file buffer and c++
 * stream classes.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLFILE_H
#define LL_LLFILE_H

/**
 * This class provides a cross platform interface to the filesystem.
 * Attempts to mostly mirror the POSIX style IO functions.
 */

typedef FILE	LLFILE;

#include <fstream>
#include <sys/stat.h>

#if LL_WINDOWS
// windows version of stat function and stat data structure are called _stat
typedef struct _stat	llstat;
#else
typedef struct stat		llstat;
#endif

#ifndef S_ISREG
# define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISDIR
# define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#endif

#include "llstring.h" // safe char* -> std::string conversion

class LL_COMMON_API LLFile
{
public:
	// All these functions take UTF8 path/filenames.
	static	LLFILE*	fopen(const std::string& filename,const char* accessmode);	/* Flawfinder: ignore */
	static	LLFILE*	_fsopen(const std::string& filename,const char* accessmode,int	sharingFlag);

	static	int		close(LLFILE * file);

	// perms is a permissions mask like 0777 or 0700.  In most cases it will
	// be overridden by the user's umask.  It is ignored on Windows.
	static	int		mkdir(const std::string& filename, int perms = 0700);

	static	int		rmdir(const std::string& filename);
	static	int		remove(const std::string& filename);
	static	int		rename(const std::string& filename,const std::string&	newname);
	static  bool	copy(const std::string from, const std::string to);
	static  int		size(const std::string& filename);

	static	int		stat(const std::string&	filename,llstat*	file_status);
	static	bool	isdir(const std::string&	filename);
	static	bool	isfile(const std::string&	filename);
	static	LLFILE *	_Fiopen(const std::string& filename, 
			std::ios::openmode mode);

	static  const char * tmpdir();

	// file function wrappers
	static S32 readEx(const std::string& filename, void *buf, S32 offset, S32 nbytes);
	static S32 writeEx(const std::string& filename, void *buf, S32 offset, S32 nbytes);
};

#if LL_WINDOWS
/**
*  @brief  Controlling input or output for files.
*
*  This class supports reading and writing from named files, using the inherited
*  functions from std::basic_fstream.  To control the associated
*  sequence, an instance of std::basic_filebuf (or a platform-specific derivative)
*  which allows construction using a pre-exisintg file stream buffer.
*  We refer to this std::basic_filebuf (or derivative) as @c sb.
*/
class LL_COMMON_API llfstream : public	std::fstream
{
	// file stream associated with a filebuf
public:
	// Constructors:
	/**
	*  @brief  Default constructor.
	*
	*  Initializes @c sb using its default constructor, and passes
	*  @c &sb to the base class initializer.  Does not open any files
	*  (you haven't given it a filename to open).
	*/
	llfstream();

	/**
	*  @brief  Create an input file stream.
	*  @param  Filename  String specifying the filename.
	*  @param  Mode  Open file in specified mode (see std::ios_base).
	*
	*  @c ios_base::in is automatically included in @a mode.
	*/
	explicit llfstream(const std::string& _Filename,
		ios_base::openmode _Mode = ios_base::in);
	explicit llfstream(const char* _Filename,
		ios_base::openmode _Mode = ios_base::in);

	/**
	*  @brief  Opens an external file.
	*  @param  Filename  The name of the file.
	*  @param  Node  The open mode flags.
	*
	*  Calls @c llstdio_filebuf::open(s,mode|in).  If that function
	*  fails, @c failbit is set in the stream's error state.
	*/
	void open(const std::string& _Filename,
		ios_base::openmode _Mode = ios_base::in);
	void open(const char* _Filename,
		ios_base::openmode _Mode = ios_base::in);
};

/**
 *  @brief  Controlling input for files.
 *
 *  This class supports reading from named files, using the inherited
 *  functions from std::basic_istream.  To control the associated
 *  sequence, an instance of std::basic_filebuf (or a platform-specific derivative)
 *  which allows construction using a pre-exisintg file stream buffer. 
 *  We refer to this std::basic_filebuf (or derivative) as @c sb.
 */
class LL_COMMON_API llifstream : public	std::ifstream
{
	// input stream associated with a C stream
public:
	// Constructors:
	/**
	*  @brief  Default constructor.
	*
	*  Initializes @c sb using its default constructor, and passes
	*  @c &sb to the base class initializer.  Does not open any files
	*  (you haven't given it a filename to open).
	*/
	llifstream();

	/**
	*  @brief  Create an input file stream.
	*  @param  Filename  String specifying the filename.
	*  @param  Mode  Open file in specified mode (see std::ios_base).
	*
	*  @c ios_base::in is automatically included in @a mode.
	*/
	explicit llifstream(const std::string& _Filename,
		ios_base::openmode _Mode = ios_base::in);
	explicit llifstream(const char* _Filename,
		ios_base::openmode _Mode = ios_base::in);

	/**
	*  @brief  Opens an external file.
	*  @param  Filename  The name of the file.
	*  @param  Node  The open mode flags.
	*
	*  Calls @c llstdio_filebuf::open(s,mode|in).  If that function
	*  fails, @c failbit is set in the stream's error state.
	*/
	void open(const std::string& _Filename,
		ios_base::openmode _Mode = ios_base::in);
	void open(const char* _Filename,
		ios_base::openmode _Mode = ios_base::in);
};


/**
 *  @brief  Controlling output for files.
 *
 *  This class supports writing to named files, using the inherited
 *  functions from std::basic_ostream.  To control the associated
 *  sequence, an instance of std::basic_filebuf (or a platform-specific derivative)
 *  which allows construction using a pre-exisintg file stream buffer. 
 *  We refer to this std::basic_filebuf (or derivative) as @c sb.
*/
class LL_COMMON_API llofstream : public	std::ofstream
{
public:
	// Constructors:
	/**
	*  @brief  Default constructor.
	*
	*  Initializes @c sb using its default constructor, and passes
	*  @c &sb to the base class initializer.  Does not open any files
	*  (you haven't given it a filename to open).
	*/
	llofstream();

	/**
	*  @brief  Create an output file stream.
	*  @param  Filename  String specifying the filename.
	*  @param  Mode  Open file in specified mode (see std::ios_base).
	*
	*  @c ios_base::out is automatically included in @a mode.
	*/
	explicit llofstream(const std::string& _Filename,
		ios_base::openmode _Mode = ios_base::out | ios_base::trunc);
	explicit llofstream(const char* _Filename,
		ios_base::openmode _Mode = ios_base::out | ios_base::trunc);

	/**
	*  @brief  Opens an external file.
	*  @param  Filename  The name of the file.
	*  @param  Node  The open mode flags.
	*
	*  Calls @c llstdio_filebuf::open(s,mode|out).  If that function
	*  fails, @c failbit is set in the stream's error state.
	*/
	void open(const std::string& _Filename,
		ios_base::openmode _Mode = ios_base::out | ios_base::trunc);
	void open(const char* _Filename,
		ios_base::openmode _Mode = ios_base::out | ios_base::trunc);
};

/**
 * @breif filesize helpers.
 *
 * The file size helpers are not considered particularly efficient,
 * and should only be used for config files and the like -- not in a
 * loop.
 */
std::streamsize LL_COMMON_API llifstream_size(llifstream& fstr);
std::streamsize LL_COMMON_API llofstream_size(llofstream& fstr);

#else // ! LL_WINDOWS

// on non-windows, llifstream and llofstream are just mapped directly to the std:: equivalents
typedef std::fstream  llfstream;
typedef std::ifstream llifstream;
typedef std::ofstream llofstream;

#endif // LL_WINDOWS or ! LL_WINDOWS

#endif // not LL_LLFILE_H
