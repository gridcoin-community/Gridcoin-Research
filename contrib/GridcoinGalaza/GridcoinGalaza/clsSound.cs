// Purpose:      FMOD
// Rev:          1.0
// Last updated: 22/03/10

using System;
using System.Runtime.InteropServices;

namespace GridcoinGalaza
{
    public class clsSound
    {
        private int fmodHandle;
        private int channel;
        const int FSOUND_FREE = -1;
        const int FSOUND_ALL = -3;
        private string playFile = "";
        System.Media.SoundPlayer _player = new System.Media.SoundPlayer();
        WMPLib.WindowsMediaPlayer _wmPlayer = new WMPLib.WindowsMediaPlayer();
        WMPLib.WindowsMediaPlayer _wmPlayer2 = new WMPLib.WindowsMediaPlayer();
        private DateTime lastcall = System.DateTime.Now;
        bool PlayerToggle = false;
        public bool SoundEnabled = false;
        public bool LOOP = false;


        public clsSound(string filename, bool EnableSound)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Class constructor  
            //------------------------------------------------------------------------------------------------------------------

            playFile = filename;
            SoundEnabled = EnableSound;
        }

        public void playSND(bool loopSND)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method (play sound)  
            //------------------------------------------------------------------------------------------------------------------

            if (!SoundEnabled) return;

        
            if (loopSND)
            {
                _player.SoundLocation = playFile;
                _player.PlayLooping();
            }
            else
            {
                double elapsed = System.DateTime.Now.Subtract(lastcall).TotalMilliseconds;
                PlayerToggle = !PlayerToggle;
                if (elapsed > 275)
                {
                    if (PlayerToggle)
                    {
                        _wmPlayer.URL = @playFile;
                        lastcall = System.DateTime.Now;
                    }
                    else
                    {
                        _wmPlayer2.URL = @playFile;
                        lastcall = System.DateTime.Now;
                
                    }
                }

                
            }

            

        }

        public void stopSND()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method (cease sound)  
            //------------------------------------------------------------------------------------------------------------------
            _player.Stop();

        }

        public bool isPlaying()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: (is channel playing?)   
            //------------------------------------------------------------------------------------------------------------------
            
            return false;

        }
    }
}
