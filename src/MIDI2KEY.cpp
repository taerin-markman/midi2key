
/****************************************************************************\

                          D E C L A R A T I O N S

\****************************************************************************/
/****************************************************************************\
                              HEADER INCLUDES
\****************************************************************************/
#include "stdafx.h"
#include <stdio.h>
#include <msacm.h>
#include "MIDI2KEY.h"

/****************************************************************************\
                                DEBUG CODE
\****************************************************************************/
#ifdef _DEBUG
void PlayerDebug(const char * const file, int line, const char * const text)
{
	char fmt[] = "%s %d: %s\n";
	char i[255];
	_snprintf_s(i,254,fmt,file,line,text);
	OutputDebugString(i);
}
#endif

/****************************************************************************\
                             TYPE DEFINITIONS
\****************************************************************************/
/* This is a generic way to deconstruct dwParam1 into a MIDI event. The 
   "event" will determine what param1 and param2 do. At that point, you can
   cast this to one of the more specific types. */
typedef struct {
  DWORD channel:4;
  DWORD event:4;
  DWORD param1:8;
  DWORD param2:8;
} midiEventType;

/* A more specific deconstruction of dwParam1 from the MIDI driver for use in
   both note on and note off events. */
typedef struct {
  DWORD channel:4;
  DWORD event:4;
  DWORD note:8;
  DWORD velocity:8;
} midiNoteOnEventType;

typedef char keyboardType;

typedef unsigned char midiNoteType;

/* Used to store status of each note, including the keyboard scancode
   translation for that note. */
typedef struct {
  keyboardType  key;
  bool          on;
  DWORD         lasttime;  
} midiNoteStatusType;

/* Used to store the status of each key. */
typedef struct {
  bool  depressed;
  DWORD lasttime;
} keyboardNoteStatusType;

/****************************************************************************\
                     VARIABLE AND MACRO DECLARATIONS
\****************************************************************************/
/* Filters MIDI events for the same note that are less this many milliseconds
   apart */
#define TIME_ELAPSED_MINIMUM_GRANULARITY 100

#define ARPEGGIATOR_TIMER_GRANULARITY 180

/* Maximum number of "channels" supported in MIDI protocol. */
#define MAX_MIDI_CHANNELS 16

/* On a range from 0-15, MIDI drum channel is... */
#define MIDI_DRUM_CHANNEL 9

/* Constant for maximum value of a DWORD */
#define MAX_DWORD ((DWORD)(-1))

/* Used for window class and title string sizes. */
#define MAX_LOADSTRING        100

/* Can't imagine having more than 50 MIDI input devices in a system. (wow!)
   This controls the max we can support since we're using static allocators
   for each MIDI device in the system. */
#define MAX_SUPPORTED_DEVICES 50

/* Update this whenever you change something. This string is written to the
   window on startup. Don't forget to change the VERSION resources as well. */
#define MIDI2KEY_VERSION_TEXT    "v0.9.0.5"

#define MIDI2KEY_APP_NAME        "MIDI2KEY"

/* These are MIDI note values. N_C is middle-C. These should not vary from
   keyboard to keyboard, so these should never change. */
#define N_C  (0x3c)
#define N_CS (N_C+1)
#define N_D  (N_C+2)
#define N_DS (N_C+3)
#define N_E  (N_C+4)
#define N_ES (N_C+5)
#define N_F  (N_C+5)
#define N_FS (N_C+6)
#define N_G  (N_C+7)
#define N_GS (N_C+8)
#define N_A  (N_C+9)
#define N_AS (N_C+10)
#define N_B  (N_C+11)
#define N_BS  (N_C+12)

/* Since I'm too lazy to actually write out the MIDI note values for each
   octave as above, I use this macro to "offset" the octave given the note
   as identified by one of the macros above (n parameter), and a signed
   value indicating the relative octave from that (o parameter). For
   example:

   OCTAVE(N_DS,-1) is the same as what we'd otherwise get by doing N_DS, but
   one octave lower. WARNING: No bounds checking! (OMG, heathen!) */
#define OCTAVE(n,o) ((n)+(12*(o)))



/* Windows instance, title, class, handle garbage. */
HINSTANCE hInst;
TCHAR     szTitle[MAX_LOADSTRING];
TCHAR     szWindowClass[MAX_LOADSTRING];
HWND      hWndMain, hWndDialog;

/* MIDI device handles. Could be several since we just cart-blanche open all
   MIDI input devices. */
HMIDIIN hMidiIn[MAX_SUPPORTED_DEVICES];

/* Identifies which MIDI input devices we've opened. */
bool devOpen[MAX_SUPPORTED_DEVICES] = {false};

/* Gets filled with the number of MIDI input devices in the system. */
unsigned int numDevices = 0;

/* Oh boy how I love systems with unlimited memory! (kidding) Since we know
   that a MIDI note can only occupy 8-bits, I figure why not just create a
   static table capable of mapping all possible MIDI notes to windows key
   events? So I did. And here it is. */
midiNoteStatusType midiNoteStatus[MAX_MIDI_CHANNELS][0xff];

/* Stores keyboard status for each note in the LOTRO repertoire. */
keyboardNoteStatusType keyboardNoteStatus[256] = {0};

/* Arpeggiator timer ID constant. */
const unsigned int IDT_ARPEGGIATOR = 1;

/* Enables Arpeggiator or not. */
bool arpEnabled = false;

/* Keeps track of the last note played for walking purposes. */
midiNoteType arpLastNote = 0;

/* Controls whether high-octave notes are part of the arpeggiator or not. */
bool arpFreehandHighOctave = false;

/* Arpeggiator walks down by default. arpCrawlUp == true causes it to walk
   up instead. */
bool arpCrawlUp = false;

midiNoteType maxMIDINote = 0;
midiNoteType minMIDINote = 0xff;
midiNoteType numMIDINotes = 0;

midiNoteType arpeggiatorKeyboardSplit = 0;

/****************************************************************************\
                  LOCAL FUNCTION FORWARD-DECLARATIONS
\****************************************************************************/
/* Windows schtuff. Don't ask me. I don't know either. */
ATOM				      MyRegisterClass( HINSTANCE hInstance );
BOOL				      InitInstance( HINSTANCE, int );
LRESULT CALLBACK	WndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT CALLBACK	MainDialog( HWND, UINT, WPARAM, LPARAM );

/* MIDI Input device control */
bool midiOpenDevice( unsigned int devid );
void midiCloseDevices( void );
void midiEnumerateDeviceList( void );

/* Sends a keyboard key event to the active application. */
void GenerateKeyScancode ( UINT vk , bool down, bool bExtended );


#define SEND_KEYDOWN(bIsVkey,key) \
  GenerateKeyScancode(            \
    MapVirtualKey(                \
      ((bIsVkey) ?                \
       (key) :                    \
       (0xff & VkKeyScan((key)))  \
      ),                          \
      0                           \
    ),                            \
    true,                         \
    false                         \
  )

#define SEND_KEYUP(bIsVkey,key) \
  GenerateKeyScancode(          \
    MapVirtualKey(              \
      ((bIsVkey) ?              \
       (key) :                  \
       (0xff & VkKeyScan((key)))\
      ),                        \
      0                         \
    ),                          \
    false,                      \
    false                       \
  )

/****************************************************************************\

                F U N C T I O N   D E F I N I T I O N S

\****************************************************************************/

/*
** FUNCTION SetMIDIKeyValue
**
** DESCRIPTION
**   Sets MIDI note to key mapping, individually.
**
*****************************************************************************/
void SetMIDIKeyValue( midiNoteType midiNote, char channel, char key )
{
  if( key != 0 )
  {
    if( midiNoteStatus[channel][midiNote].key == 0 )
    {
      if( midiNote > maxMIDINote )
      {
        maxMIDINote = midiNote;
      }

      if( midiNote < minMIDINote )
      {
        minMIDINote = midiNote;
      }

      numMIDINotes++;
    }
  }
  else
  {
    if( midiNoteStatus[channel][midiNote].key != 0 )
    {
      if( midiNote == maxMIDINote )
      {
        midiNoteType m = (midiNote > 0 ? midiNote - 1 : 0);

        while( midiNoteStatus[channel][m--].key == 0 
            && m != 0 );

        maxMIDINote = m;
      }
      
      if( midiNote == minMIDINote )
      {
        midiNoteType m = (midiNote < 0xff ? midiNote + 1 : 0xff);

        while( midiNoteStatus[channel][m++].key == 0
            && m != 0xff );

        minMIDINote = m;
      }

      numMIDINotes--;
    }
  }


  midiNoteStatus[channel][midiNote].key = key;
}

/*
** FUNCTION EnableArpeggiator
**
** DESCRIPTION
**   Enables (or disables) the arpeggiator.
**
*****************************************************************************/
void EnableArpeggiator( bool enable )
{
  if( enable && !arpEnabled )
  {
    arpEnabled = true;
    arpLastNote = 0;

    SetTimer(hWndDialog,
      IDT_ARPEGGIATOR,
      ARPEGGIATOR_TIMER_GRANULARITY,
      (TIMERPROC) NULL
      );
  }
  else if ( !enable && arpEnabled )
  {
    arpEnabled = false;
    KillTimer( hWndDialog, IDT_ARPEGGIATOR );
  }
}

/*
** FUNCTION DoArpeggiator
**
** DESCRIPTION
**   Process arpeggiator state and output.
**
*****************************************************************************/
void DoArpeggiator()
{
  unsigned int channel = 0; //??
  midiNoteType midiNote = 0;
  midiNoteType midiStartNote = 0;

  midiNoteType startTraverse, endTraverse;

  if( arpEnabled )
  {
    if( !arpCrawlUp )
    {
      if( !arpFreehandHighOctave )
      {
        startTraverse = 0xff;
      }
      else
      {
        startTraverse = arpeggiatorKeyboardSplit;
      }
      endTraverse = 0;
    }
    else
    {
      if( !arpFreehandHighOctave )
      {
        endTraverse = 0xff;
      }
      else
      {
        endTraverse = arpeggiatorKeyboardSplit;
      }
      startTraverse = 0;
    }

    if( !arpCrawlUp )
    {
      for( midiNoteType i = startTraverse - 1; i > endTraverse; i-- )
      {
        if( midiNoteStatus[channel][i].key != 0
         && keyboardNoteStatus[midiNoteStatus[channel][i].key].depressed )
        {
          if( midiStartNote == 0 )
          {
            midiStartNote = i;
          }

          if( arpLastNote == 0
          || i < arpLastNote )
          {
            midiNote = i;
            break;
          }
        }
      }
    }
    else
    {
      for( midiNoteType i = startTraverse; i < endTraverse; i++ )
      {
        if( midiNoteStatus[channel][i].key != 0
         && keyboardNoteStatus[midiNoteStatus[channel][i].key].depressed )
        {
          if( midiStartNote == 0 )
          {
            midiStartNote = i;
          }

          if( arpLastNote == 0
          || i > arpLastNote )
          {
            midiNote = i;
            break;
          }
        }
      }
    }

    if( midiNote == 0 )
    {
      midiNote = midiStartNote;
    }

    if( midiNote != 0 )
    {
      arpLastNote = midiNote;

      SEND_KEYDOWN(false,midiNoteStatus[channel][midiNote].key);

      Sleep(20L);

      SEND_KEYUP(false,midiNoteStatus[channel][midiNote].key);
    }

    SetTimer(hWndDialog,
      IDT_ARPEGGIATOR,
      ARPEGGIATOR_TIMER_GRANULARITY,
      (TIMERPROC) NULL
      );
  }
}

/*
** FUNCTION GetNoteSharpStatus
**
** DESCRIPTION
**   Returns true if note is a sharp.
**
*****************************************************************************/
bool GetNoteSharpStatus( char note )
{
  bool status = false;

  switch( note % 12 )
  {
  case 1:
  case 3:
  case 6:
  case 8:
  case 10:
    status = true;
    break;
  default:
    break;

  }
  
  return status;
}


/*
** FUNCTION GetNoteVkey
**
** DESCRIPTION
**   Retrieves the note's natural VK_ identifier.
**
*****************************************************************************/
unsigned int GetNoteVkey( char note )
{
  unsigned int vkey = 0;

  switch( note % 12 )
  {
  case 0:
  case 1:
    vkey = (unsigned int)'C';
    break;
  case 2:
  case 3:
    vkey = (unsigned int)'D';
    break;
  case 4:
    vkey = (unsigned int)'E';
    break;
  case 5:
  case 6:
    vkey = (unsigned int)'F';
    break;
  case 7:
  case 8:
    vkey = (unsigned int)'G';
    break;
  case 9:
  case 10:
    vkey = (unsigned int)'A';
    break;
  case 11:
    vkey = (unsigned int)'B';
    break;
  default:
    vkey = (unsigned int)'X';
    break;

  }
  
  return vkey;
}


/*
** FUNCTION StrToMIDINote
**
** DESCRIPTION
**   Converts a string to a MIDI note value.
**
*****************************************************************************/
unsigned int StrToMIDINote( const char * str )
{
  unsigned int midiNote = 256;

  bool sharp = false;
  unsigned int octaveVal = 0;
  char octaveChar = 0;
  size_t strSize = strlen( str );

  if( strSize == 3 )
  {
    if( str[1] == 'S' )
    {
      sharp = true;
    }
    else
    {
      return midiNote;
    }
    octaveChar = str[2];
  }
  else if( strSize == 2 )
  {
    octaveChar = str[1];
  }
  else
  {
    return 256;
  }


  if( octaveChar >= '0' && octaveChar <= '9' )
  {
    octaveVal = octaveChar - '0';
  }
  else
  {
    return 256;
  }
  
  switch( str[0] )
  {
  case 'C':
  case 'c':
    midiNote = OCTAVE((sharp ? N_CS : N_C),(int)octaveVal - 3);
    break;
  case 'D':
  case 'd':
    midiNote = OCTAVE((sharp ? N_DS : N_D),(int)octaveVal - 3);
    break;
  case 'E':
  case 'e':
    midiNote = OCTAVE((sharp ? N_ES : N_E),(int)octaveVal - 3);
    break;
  case 'F':
  case 'f':
    midiNote = OCTAVE((sharp ? N_FS : N_F),(int)octaveVal - 3);
    break;
  case 'G':
  case 'g':
    midiNote = OCTAVE((sharp ? N_GS : N_G),(int)octaveVal - 3);
    break;
  case 'A':
  case 'a':
    midiNote = OCTAVE((sharp ? N_AS : N_A),(int)octaveVal - 3);
    break;
  case 'B':
  case 'b':
    midiNote = OCTAVE((sharp ? N_BS : N_B),(int)octaveVal - 3);
    break;
  default:
    return midiNote;
  }

  return midiNote;
}


/*
** FUNCTION ReadSettings
**
** DESCRIPTION
**   Reads and applies settings from ini file.
**
*****************************************************************************/
void ReadSettings()
{
  /* This needs to be able to contain all of the keys of the key/value pairs
     in the [Key Map] section of the ini file. */
  char returnString[5000] = "";

  DWORD numBytes = 0, curPos = 0;

  /* Retrieve all of the keys in the [Key Map] section of the ini file. */
  numBytes = GetPrivateProfileString(
    "Key Map",
    NULL,
    "",
    returnString,
    5000,
    ".\\MIDI2KEY.ini"
  );


  if( numBytes > 0 )
  {
    /* Iterate through each key. */
    while( curPos < numBytes )
    {
      char          keyString[2] = "";
      DWORD         numBytesLotro = 0;
      unsigned int  midiNote = 0;
      char          key = 0;
      char          channel = MAX_MIDI_CHANNELS;

      /* Translate the key name into a MIDI note. */
      midiNote = StrToMIDINote(returnString + curPos);

      if( midiNote < 256 )
      {
        /* Fetch the value of this key, which will become the keyboard key
           for that MIDI note. */
        numBytesLotro = GetPrivateProfileString(
          "Key Map",
          returnString + curPos,
          "",
          keyString,
          2,
          ".\\MIDI2KEY.ini"
        );

        if( numBytesLotro > 0 )
        {
          key = keyString[0];
        }

        while( channel-- )
        {
          if( channel != MIDI_DRUM_CHANNEL )
          {
            continue;
          }
          SetMIDIKeyValue( (midiNoteType)midiNote, channel, key );
        }
      }

      /* Advance to the next key in the string. */
      curPos += strlen(returnString + curPos) + 1;
    }
  }

  /* TODO: Read other settings here. */
  arpeggiatorKeyboardSplit = OCTAVE( N_C, 1 );
}

/*
** FUNCTION SaveSettings
**
** DESCRIPTION
**   Reads and applies settings from ini file.
**
*****************************************************************************/
void SaveSettings()
{
#if 0
  BOOL success = WritePrivateProfileString(
    "Settings",
    "AlternateOctaves",
    "0",
    ".\\MIDI2KEY.ini"
  );
#endif /* 0 */
}

/*
** FUNCTION SetKeyMap
**
** DESCRIPTION
**   Sets MIDI note to key mapping.
**
*****************************************************************************/
void SetKeyMap()
{
  char channel = MAX_MIDI_CHANNELS;

  /* Woohoo, magic letters and numbers galore! Yes, I am l33t programmer!
     *flex* Ok seriously, this is where I'm creating the mappings from
     MIDI event notes to keyboard events. Note that there are only a
     limited number of them. */

  while( channel-- )
  {
    if( channel != MIDI_DRUM_CHANNEL )
    {
      continue;
    }

    /* Octave two below middle-C. */
    SetMIDIKeyValue( OCTAVE(N_C,-2), channel, 'A' );
    SetMIDIKeyValue( OCTAVE(N_CS,-2), channel, 'W' );
    SetMIDIKeyValue( OCTAVE(N_D,-2), channel, 'S' );
    SetMIDIKeyValue( OCTAVE(N_DS,-2), channel, 'E' );
    SetMIDIKeyValue( OCTAVE(N_E,-2), channel, 'D' );
    SetMIDIKeyValue( OCTAVE(N_F,-2), channel, 'F' );
    SetMIDIKeyValue( OCTAVE(N_FS,-2), channel, 'T' );
    SetMIDIKeyValue( OCTAVE(N_G,-2), channel, 'G' );
    SetMIDIKeyValue( OCTAVE(N_GS,-2), channel, 'Y' );
    SetMIDIKeyValue( OCTAVE(N_A,-2), channel, 'H' );
    SetMIDIKeyValue( OCTAVE(N_AS,-2), channel, 'U' );
    SetMIDIKeyValue( OCTAVE(N_B,-2), channel, 'Q' );

    /* Octave one below middle-C. */
    SetMIDIKeyValue( OCTAVE(N_C,-1), channel, 'A' );
    SetMIDIKeyValue( OCTAVE(N_CS,-1), channel, 'W' );
    SetMIDIKeyValue( OCTAVE(N_D,-1), channel, 'S' );
    SetMIDIKeyValue( OCTAVE(N_DS,-1), channel, 'E' );
    SetMIDIKeyValue( OCTAVE(N_E,-1), channel, 'D' );
    SetMIDIKeyValue( OCTAVE(N_F,-1), channel, 'F' );
    SetMIDIKeyValue( OCTAVE(N_FS,-1), channel, 'T' );
    SetMIDIKeyValue( OCTAVE(N_G,-1), channel, 'G' );
    SetMIDIKeyValue( OCTAVE(N_GS,-1), channel, 'Y' );
    SetMIDIKeyValue( OCTAVE(N_A,-1), channel, 'H' );
    SetMIDIKeyValue( OCTAVE(N_AS,-1), channel, 'U' );
    SetMIDIKeyValue( OCTAVE(N_B,-1), channel, 'Q' );

    /* Middle-C octave. */
    SetMIDIKeyValue( N_C, channel, 'A' );
    SetMIDIKeyValue( N_CS, channel, 'W' );
    SetMIDIKeyValue( N_D, channel, 'S' );
    SetMIDIKeyValue( N_DS, channel, 'E' );
    SetMIDIKeyValue( N_E, channel, 'D' );
    SetMIDIKeyValue( N_F, channel, 'F' );
    SetMIDIKeyValue( N_FS, channel, 'T' );
    SetMIDIKeyValue( N_G, channel, 'G' );
    SetMIDIKeyValue( N_GS, channel, 'Y' );
    SetMIDIKeyValue( N_A, channel, 'H' );
    SetMIDIKeyValue( N_AS, channel, 'U' );
    SetMIDIKeyValue( N_B, channel, 'J' );

    /* Octave one above middle-C. */
    SetMIDIKeyValue( OCTAVE(N_C,1), channel, '1' );
    SetMIDIKeyValue( OCTAVE(N_CS,1), channel, 'W' );
    SetMIDIKeyValue( OCTAVE(N_D,1), channel, '2' );
    SetMIDIKeyValue( OCTAVE(N_DS,1), channel, 'E' );
    SetMIDIKeyValue( OCTAVE(N_E,1), channel, '3' );
    SetMIDIKeyValue( OCTAVE(N_F,1), channel, '4' );
    SetMIDIKeyValue( OCTAVE(N_FS,1), channel, 'T' );
    SetMIDIKeyValue( OCTAVE(N_G,1), channel, '5' );
    SetMIDIKeyValue( OCTAVE(N_GS,1), channel, 'Y' );
    SetMIDIKeyValue( OCTAVE(N_A,1), channel, '6' );
    SetMIDIKeyValue( OCTAVE(N_AS,1), channel, 'U' );
    SetMIDIKeyValue( OCTAVE(N_B,1), channel, '7' );

    /* Octave two above middle-C. */
    SetMIDIKeyValue( OCTAVE(N_C,2), channel, '8' );
    SetMIDIKeyValue( OCTAVE(N_CS,2), channel, 'W' );
    SetMIDIKeyValue( OCTAVE(N_D,2), channel, '2' );
    SetMIDIKeyValue( OCTAVE(N_DS,2), channel, 'E' );
    SetMIDIKeyValue( OCTAVE(N_E,2), channel, '3' );
    SetMIDIKeyValue( OCTAVE(N_F,2), channel, '4' );
    SetMIDIKeyValue( OCTAVE(N_FS,2), channel, 'T' );
    SetMIDIKeyValue( OCTAVE(N_G,2), channel, '5' );
    SetMIDIKeyValue( OCTAVE(N_GS,2), channel, 'Y' );
    SetMIDIKeyValue( OCTAVE(N_A,2), channel, '6' );
    SetMIDIKeyValue( OCTAVE(N_AS,2), channel, 'U' );
    SetMIDIKeyValue( OCTAVE(N_B,2), channel, '7' );
  }

  ReadSettings();

}

/*
** FUNCTION WinMain
**
** DESCRIPTION
**   Program entry-point.
**
*****************************************************************************/
int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	MSG msg;
	HACCEL hAccelTable;

  ZeroMemory(midiNoteStatus,
             sizeof(midiNoteStatusType) * 0xff * MAX_MIDI_CHANNELS);

	/* Windows garbage. Don't ask me, I don't know either. */
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_SWGSONGSCOREWIN32, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	/* Create the instance for this utility. */
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

  /* Set the initial keymap. */
  SetKeyMap();

  /* Load accelerators. Why? I don't know. Future expansion, maybe. */
	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_SWGSONGSCOREWIN32);

	/* Main message loop. This is the ubah windows messaging core, baby! */
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(hWndDialog, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}


/*
** FUNCTION MyRegisterClass
**
** DESCRIPTION
**   Registers window class for the dialog.
**
*****************************************************************************/
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize         = sizeof(WNDCLASSEX); 

	wcex.style			    = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	  = (WNDPROC)WndProc;
	wcex.cbClsExtra		  = 0;
	wcex.cbWndExtra		  = 0;
	wcex.hInstance		  = hInstance;
	wcex.hIcon			    = NULL; //LoadIcon(hInstance, (LPCTSTR)IDI_MYICONHERE);
	wcex.hCursor		    = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	  = (LPCTSTR)IDC_SWGSONGSCOREWIN32;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		    = NULL; //LoadIcon(hInstance, (LPCTSTR)IDI_MYICONHERE);

	return RegisterClassEx(&wcex);
}


/*
** FUNCTION InitInstance
**
** DESCRIPTION
**   Called by WinMain to initialize the application instance. Enumerates and
**   opens the MIDI input devices, creates the dialog (since I am a dialog
**   fiend and not a regular window type of person), and generally creates
**   all other manner of mayhem.
**
*****************************************************************************/
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
  hInst = hInstance;

  /* CreateWindow does something that has to do with creating a window with
     an un-godly number of parameters. I don't think any "window" is actually
     even created here, though. That's one later via the CreateDialogParam()
     call. This is used simply to appease the God of Windows. */
  hWndMain = CreateWindow(
    szWindowClass,
    szTitle,
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT,
    0,
    CW_USEDEFAULT,
    0,
    NULL,
    NULL,
    hInstance,
    NULL );

  /* Couldn't create. Go BOOM. (note: a user message might be useful here) */
  if (!hWndMain)
  {
    return FALSE;
  }

  /* Enumerate and open the MIDI devices. */
  numDevices = midiInGetNumDevs();
  if( numDevices < 1 )
  {
    MessageBoxEx(hWndMain,"ERROR: Sorry about this, but I can't seem to find any MIDI input devices installed in your computer. My advice: check to make sure you have drivers for it installed properly. Beyond that, I can't help ya, I'm afraid. :-(","No MIDI Input Devices",MB_ICONSTOP | MB_OK,NULL);
		return FALSE;
  }

  /* Cap the # of devices to max, just in case. */
  if( numDevices > MAX_SUPPORTED_DEVICES )
  {
    MessageBoxEx(hWndMain,"NOTE: Wowzer! How can you possibly have so many MIDI input devices in your system?! Anyway, I can only handle so many MIDI Input devices, so some won't be used by this utility. This may be a problem for you. Let me know that this is the case and I'll increase the capability.","Can't Use All MIDI Devices",MB_ICONINFORMATION | MB_OK,NULL);
    numDevices = MAX_SUPPORTED_DEVICES;
  }

  /* Try to open the devices and spit out informational messages if there are
     any problems */
  unsigned int numOpened = 0;

  for( unsigned int i = 0; i < numDevices; i++ )
  {
    if( midiOpenDevice(i) )
    {
      numOpened++;
    }
  }

  if( numOpened < 1 )
  {
    MessageBoxEx(hWndMain,"ERROR: Sorry about this, but, though I did find at least one MIDI Input device to use, I can't seem to get control of any of them. My advice: make sure there are no other applications running that might use MIDI. Beyond that, I can't help ya, I'm afraid. :-(","Can't Use MIDI Device",MB_ICONSTOP | MB_OK,NULL);
		return FALSE;
  }

  if( numOpened != numDevices )
  {
    MessageBoxEx(hWndMain,"NOTE: This may or may not cause a problem for you, but I was unable to get control over at least one of the MIDI Input devices in your system. I did manage to get control of at least one of them, so let's hope that it is the one you have your equipment plugged in to. :-) Otherwise, if you can't get this to work, check to make sure you don't have any other software running that would use the MIDI devices and try restarting this utility.","Can't Use a MIDI Device",MB_ICONINFORMATION | MB_OK,NULL);
  }

  /* Create the dialog.......or not. */
	hWndDialog = CreateDialogParam(
    hInst, 
    (LPCTSTR)IDD_MAINDIALOG,
    hWndMain, (DLGPROC)MainDialog,
    0 );
	if (!hWndDialog)
	{
		TRACE("Can't create dialog: %d",GetLastError());
		return FALSE;
	}

  return TRUE;
}

/*
** FUNCTION WndProc
**
** DESCRIPTION
**   Main "window" procedure for handling windows messages. Most of the work
**   won't be done here. It'll be done in the dialog message handler,
**   MainDialog().
**
*****************************************************************************/
LRESULT CALLBACK WndProc(
  HWND hWnd,
  UINT message,
  WPARAM wParam,
  LPARAM lParam
  )
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message) 
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

/*
** FUNCTION MainDialog
**
** DESCRIPTION
**   Essentially the workhorse of this application. Handles all windows
**   messages sent to the dialog.
**
*****************************************************************************/
LRESULT CALLBACK MainDialog(
  HWND hDlg,
  UINT message,
  WPARAM wParam,
  LPARAM lParam
  )
{
  bool retval = false;

	switch (message)
	{
	case WM_INITDIALOG:
    /* Update the version text on the dialog. */
		SetDlgItemText(hDlg,IDC_VERSION_TEXT,MIDI2KEY_VERSION_TEXT);

    CheckDlgButton( hDlg, IDC_ARPEGGIATOR, BST_UNCHECKED );

    CheckDlgButton( hDlg, IDC_FREEHANDHIGHOCTAVE, BST_UNCHECKED );

    /* Set the dialog's icon. Why windows doesn't use the icon we already
       specified in the window class is beyond me. */
		//hIcon = LoadIcon(hInst,MAKEINTRESOURCE(IDI_MIDI2KEY));
		//SendMessage(hDlg,WM_SETICON,(WPARAM)ICON_BIG,(LPARAM)hIcon);
		//SendMessage(hDlg,WM_SETICON,(WPARAM)ICON_SMALL,(LPARAM)hIcon);

    return TRUE;

	case WM_COMMAND:
    /* LOWORD(wParam) is the actual command ID. */
		switch (LOWORD(wParam))
		{
    case 0:
      break;

		case IDOK:
      /* User clicked "Exit" button. Be nice and shut every thing down. */
      SaveSettings();
      midiCloseDevices();
			EndDialog(hDlg, LOWORD(wParam));
			DestroyWindow(hWndMain);
			return TRUE;

    case IDC_ARPEGGIATOR:
      EnableArpeggiator(
        ( BST_CHECKED == IsDlgButtonChecked(hDlg,IDC_ARPEGGIATOR) 
          ? true : false )
        );
      break;

    case IDC_FREEHANDHIGHOCTAVE:
      arpFreehandHighOctave = 
        ( BST_CHECKED == IsDlgButtonChecked(hDlg,IDC_FREEHANDHIGHOCTAVE) 
          ? true : false );
      break;

    case IDC_ARPCRAWLUP:
      arpCrawlUp = 
        ( BST_CHECKED == IsDlgButtonChecked(hDlg,IDC_ARPCRAWLUP) 
          ? true : false );
      break;
		}
		break;

  case WM_TIMER:
    if( (unsigned int)wParam == IDT_ARPEGGIATOR )
    {
      DoArpeggiator();
    }
    break;
	}
	return retval;
}

/*
** FUNCTION GenerateKeyScancode
**
** DESCRIPTION
**   Generate a key event. Use scancodes and not virtual keys because
**   it seems LOTRO does not monitor virtual keys during a music session.
**
** CREDITS
**   Stole and adapted the basic idea from 
**   http://www.codeguru.com/forum/showthread.php?t=377393
**
*****************************************************************************/
void GenerateKeyScancode ( UINT vk , bool down, bool bExtended )
{
  KEYBDINPUT  kb={0};
  INPUT    Input={0};
  if( down )
  {
    kb.dwFlags = KEYEVENTF_SCANCODE;
    if ( bExtended )
    {
      kb.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    kb.wScan   = vk;
    Input.type = INPUT_KEYBOARD;
    Input.ki   = kb;
    SendInput( 1, &Input, sizeof( Input ) );

  }
  else
  {
    kb.dwFlags  =  KEYEVENTF_KEYUP | KEYEVENTF_SCANCODE;
    if ( bExtended )
    {
      kb.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    kb.wScan    = vk;
    Input.type  = INPUT_KEYBOARD;
    Input.ki    = kb;
    SendInput( 1, &Input, sizeof( Input ) );
  }
}


/*
** FUNCTION GenerateKeyVirtual
**
** DESCRIPTION
**   Generate a key event.
**
** CREDITS
**   Stole and adapted the basic idea from 
**   http://www.codeguru.com/forum/showthread.php?t=377393
**
*****************************************************************************/
void GenerateKeyVirtual ( UINT vk , bool down, bool bExtended )
{
  KEYBDINPUT  kb={0};
  INPUT    Input={0};
  if( down )
  {
    kb.dwFlags = 0;
    if ( bExtended )
    {
      kb.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    kb.wVk     = vk;
    Input.type = INPUT_KEYBOARD;
    Input.ki   = kb;
    SendInput( 1, &Input, sizeof( Input ) );

  }
  else
  {
    kb.dwFlags  =  KEYEVENTF_KEYUP;
    if ( bExtended )
    {
      kb.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    kb.wVk      = vk;
    Input.type  = INPUT_KEYBOARD;
    Input.ki    = kb;
    SendInput( 1, &Input, sizeof( Input ) );
  }
}


/*
** FUNCTION midiDeviceInputCallback
**
** DESCRIPTION
**   Callback from MIDI driver. This is where we receive and process
**   MIDI events as we receive them. Currently, the # of MIDI events we
**   support is very limited, both because of implementation time and because
**   there simply isn't a need to support more commands than note ON and
**   note OFF.
**
*****************************************************************************/
void CALLBACK midiDeviceInputCallback(
  HMIDIIN hMidiIn_local,
  UINT wMsg,
  DWORD dwInstance, /* we don't use (yet) -- it's basically client data */
  DWORD dwParam1,
  DWORD dwParam2
)
{
  midiEventType * data = (midiEventType *)&dwParam1;

  switch( wMsg )
  {
  case MM_MIM_CLOSE:
    TRACE("MIDI DEVICE CLOSED");
    break;
  case MM_MIM_DATA:
    /* More magic number goodness. Lemmy 'splain:
         0x8 == Note OFF
         0x9 == Note ON
         0xA == Note AFTERTOUCH
         0xB == Controller command
         0xC == Program Change
         0xD == Channel AFTERTOUCH
         0xE == Pitch Bend
    
       So basically, we only care about Note ON and Note OFF events. */
    if( data->event == 0x9 || data->event == 0x8 )
    {
      DWORD timeElapsed = 0;

      /* Convert data type for decomposition. */
      midiNoteOnEventType * noteOn = (midiNoteOnEventType *)data;

      /* Convert MIDI event note to lotroNote. */
      char key = midiNoteStatus[noteOn->channel][noteOn->note].key;

      /* Find the elapsed time for this note. */
      if( keyboardNoteStatus[key].lasttime != 0 )
      {
        DWORD tickCount = GetTickCount();
        
        /* Handle wrap-around condition (which happens each 49.7 days the OS
           is running. i.e. Very rare, but still possible. */
        if( tickCount < keyboardNoteStatus[key].lasttime )
        {
          timeElapsed = (MAX_DWORD - keyboardNoteStatus[key].lasttime)
                        + tickCount;
        }
        else
        {
          timeElapsed = tickCount - keyboardNoteStatus[key].lasttime;
        }
      }
      else
      {
        timeElapsed = MAX_DWORD;
      }

      /* Some MIDI controllers send a Note ON event with a 0 velocity
         instead of a Note OFF event for a given note. Yamaha S90ES, of all
         people, does exactly that. Strange. So anyway, we need to treat a
         0 velocity Note ON event as a Note OFF event. */
      if( noteOn->event == 0x9 && noteOn->velocity != 0 
       && noteOn->channel != 9 )
      {
        TRACE("NOTE ON");
        TRACE("|  channel: %x",noteOn->channel);
        TRACE("|     note: %x",noteOn->note);
        TRACE("| velocity: %x",noteOn->velocity);

        /* If we have a key registered for this MIDI note, send it a key
           down event for it. */
        if( key != 0 )
        {
          if( /* !midiNoteStatus[noteOn->note].on */
            !keyboardNoteStatus[key].depressed
           && timeElapsed > TIME_ELAPSED_MINIMUM_GRANULARITY )
          {
            if( !arpEnabled  
             || ( arpFreehandHighOctave
               && key >= midiNoteStatus[noteOn->channel][arpeggiatorKeyboardSplit].key )
              )
            {
              SEND_KEYDOWN(false,key);
            }

#if 0
            GenerateKeyVirtual( VK_SHIFT, true, true );
            GenerateKeyVirtual( GetNoteVkey(noteOn->note), true, false );
            GenerateKeyVirtual( VK_SHIFT, false, true );
            if( GetNoteSharpStatus( noteOn->note ) )
            {
              GenerateKeyVirtual( VK_SHIFT, true, true );
              GenerateKeyVirtual( '3', true, false );
              GenerateKeyVirtual( '3', false, false );
              GenerateKeyVirtual( VK_SHIFT, false, true );
            }
            GenerateKeyVirtual( VK_SPACE, true, true );
            GenerateKeyVirtual( VK_SPACE, false, true );
#endif
            keyboardNoteStatus[key].depressed = true;
            midiNoteStatus[noteOn->channel][noteOn->note].on = true;
          }

          //GetKeyNameText(
        }
      }
      else if( noteOn->channel != 9 )
      {
        TRACE("NOTE OFF");
        TRACE("|  channel: %x",noteOn->channel);
        TRACE("|     note: %x",noteOn->note);
        TRACE("| velocity: %x",noteOn->velocity);

        /* If we have a key registered for this MIDI note, send it a key
           up event for it. */
        if( key != 0 )
        {
          if( /* midiNoteStatus[noteOn->channel][noteOn->note].on */
            keyboardNoteStatus[key].depressed )
          {
            if( !arpEnabled 
             || ( arpFreehandHighOctave
               && key >= midiNoteStatus[noteOn->channel][arpeggiatorKeyboardSplit].key )
              )
            {
              SEND_KEYUP(false,key);
            }
            keyboardNoteStatus[key].depressed = false;
            keyboardNoteStatus[key].lasttime = GetTickCount();
            midiNoteStatus[noteOn->channel][noteOn->note].on = false;
            midiNoteStatus[noteOn->channel][noteOn->note].lasttime = GetTickCount();
          }
        }
      }
    }
    break;
  case MM_MIM_ERROR:
    TRACE("MIDI DEVICE ERROR");
    break;
  case MM_MIM_LONGDATA:
    TRACE("MIDI DEVICE LONGDATA");
    break;
  case MM_MIM_LONGERROR:
    TRACE("MIDI DEVICE LONGERROR");
    break;
  case MM_MIM_MOREDATA:
    TRACE("MIDI DEVICE MOREDATA");
    break;
  case MM_MIM_OPEN:
    TRACE("MIDI DEVICE OPEN");
    break;
  default:
    TRACE("UNKNOWN MIDI: %x %x %x",wMsg,dwParam1,dwParam2);
    break;
  }

}


/*
** FUNCTION midiCloseDevices
**
** DESCRIPTION
**   Stops and closes all open MIDI Input devices. Call this before exiting 
**   the application!
**
*****************************************************************************/
void midiCloseDevices()
{
  MMRESULT mmr;
  for( unsigned int i = 0; i < numDevices; i++ )
  {
    if ( devOpen[i] )
    {
      midiInStop(hMidiIn[i]);
      mmr = midiInClose(hMidiIn[i]);
      devOpen[i] = false;
    }
  }
}

/*
** FUNCTION midiOpenDevice
**
** DESCRIPTION
**   Opens and starts capture on the given MIDI Input device. Devices are
**   enumerated from 0 to one less than the number of devices in the system.
**
*****************************************************************************/
bool midiOpenDevice( unsigned int devid )
{
  MMRESULT mmr;
  if ( !devOpen[devid] )
  {
    mmr = midiInOpen(
      &hMidiIn[devid],          
      devid,
      (DWORD_PTR)&midiDeviceInputCallback,
      NULL, /* client data, which we don't need (yet) */
      CALLBACK_FUNCTION
    );
    if( mmr == MMSYSERR_NOERROR )
    {
      devOpen[devid] = true;

      mmr = midiInStart(hMidiIn[devid]);
    }
    else
    {
      return FALSE;
    }
  }
  return TRUE;
}

/*
** FUNCTION midiEnumerateDeviceList
**
** DESCRIPTION
**   Enumerate the MIDI Input devices. Need to implement this function
**   eventually. Not really necessary at the moment, though, since DevCaps
**   for MIDI input devices don't really contain any capability data--just
**   manufacturer strings and the like.
**
*****************************************************************************/
void midiEnumerateDeviceList()
{
#if 0
  MIDIINCAPS midiInCaps;
  for( unsigned int i = 0; i < numDevices; i++ )
  {
    MMRESULT mmr = midiInGetDevCaps(i,&midiInCaps,sizeof(midiInCaps));
    // print stuff out, populate selectable list
  }
#endif /* 0 */
}