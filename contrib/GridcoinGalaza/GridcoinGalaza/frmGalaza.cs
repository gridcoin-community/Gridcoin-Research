using System;
using System.Drawing;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.IO;
using System.Media;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.Drawing.Drawing2D;

namespace GridcoinGalaza
{

    // ______________________________________________________________________________
    //            Copyright ©2008-2010 Trent Jackson All Rights Reserved  
    //            Email: trentjackson888@bigpond.com.au
    //            Changes: Rob Halford: added double wide ship, alien abductions, black star field, transparent shield, transparent ammo, removed alternating background, added abduction logic, changed score system, changed explosion, changed life BL, added windows compatible sound, added abduction sound and changed warp
    // ______________________________________________________________________________

    // Product:  Video game
    // Release:  Beta     
    // Rveision: b 1.0 (World ED)
    // Platform: Windows XP .NET C#
    // Date:     March 22, 2010

    // TO DO:
    // 1. Modernize invaders and player's ship graphics
    // 2. Joystick support ...
    // 3. Menu with user-definable options

    // TERMS OF USE:
    // This program is free software. Redistribution in the form of source code only, 
    // strictly for non-profit purposes, with or without modification is permitted. 
    // The author accepts no liability for anything that may result from the usage of 
    // this product. This notice must not be removed or altered. 
    // ______________________________________________________________________________


    public partial class frmGalaza : Form
    {

        public int ScreenWidth = 1000;
        public int ScreenHeight = 900;
        public clsCommon _clsCommon = new clsCommon(0, 0, 0, 0);
   

        // APIs native Win32
        [DllImport("user32", CharSet = CharSet.Ansi, SetLastError = true, ExactSpelling = true)]
        private static extern short GetAsyncKeyState(int vkey);
        [DllImport("winmm.dll", CharSet = CharSet.Ansi, SetLastError = true, ExactSpelling = true)]
        private static extern int timeGetTime();
        [DllImport("winmm.dll", CharSet = CharSet.Ansi, SetLastError = true, ExactSpelling = true)]
        private static extern int timeBeginPeriod(int uPeriod);
        [DllImport("fmod.dll", EntryPoint = "_FSOUND_Init@12", CharSet = CharSet.Ansi, SetLastError = true, ExactSpelling = true)]
        private static extern byte FSOUND_Init(int mixrate, Int32 maxchannels, int flags);
        [DllImport("fmod.dll", EntryPoint = "_FSOUND_Close@0", CharSet = CharSet.Ansi, SetLastError = true, ExactSpelling = true)]
        private static extern int FSOUND_Close();

        // Screen buffer 
        private ImageAttributes ImageAtt = new ImageAttributes();
        private System.Drawing.Bitmap scrnBufferBmp;
        private System.Drawing.Graphics graphicsBuffer;

        // Graphic refs ...
        private System.Drawing.Bitmap[] background = new System.Drawing.Bitmap[7];
        private System.Drawing.Bitmap gameTitle;
        private System.Drawing.Bitmap star;
        private System.Drawing.Bitmap keyInstructions;

        private Random randNum = new Random();
        private bool SoundEnabled = false;

        //:: Game variables ::

        // Integers ...
        private int processStartmS;
        private int delayNextShot;
        private int tInvadersLaunched;
        private int[] entryPathIndex = new int[7];
        private int releaseNewInvader;
        private int invadersFinishedPath;
        private int invaderBatchStartIndex;
        private int invaderEntryPath;
        private int newBatchDelay;
        private int tInvadersInPath;
        private int genWorkingCounter;
        private int tDiversInPath;
        private int tShootersInPath;
        private int enPlayerSprite;
        private int level = 1;
        private int score;
        private int highScore;
        private int playerPivotPos = 61;
        private string playersName;

        // Boolean
        private bool[] moveKeyPress = new bool[3];
        private bool shootKeyPress;
        private bool shootKeyUP;
        private bool levelCompleted;
        private bool AppRunning = true;
        private bool doingSOLevel;
        private bool doingSOGame;
        private bool sndEngineError;
        private bool playerExplosion;
        private bool[] dockReserved = new bool[60];
        private bool[] dockActive = new bool[60];

        // Strings
        private string[] Top5PlayerName = new string[6];
        private string[] Top5PlayerScore = new string[6];

        // Class refs
        private clsVectorScroll[] vectorScroll = new clsVectorScroll[10];
        private clsPlayerBullet[,] playerbullet = new clsPlayerBullet[6,15];
        private clsInvader[] invaders = new clsInvader[60];
        private clsParticle[] particle = new clsParticle[60];
        private clsStars[] stars = new clsStars[100];
        private clsPlayer player;
        private clsPickups pickup;
        private clsPaths paths;
        private clsSound[] bkMusic = new clsSound[2];
        private clsTScales tScales = new clsTScales();

        // Collision detection usage
        private Rectangle rect1 = new Rectangle();
        private Rectangle rect2 = new Rectangle();
        private Rectangle[] dockRect = new Rectangle[60];

        // Constants
        const double PI = 3.141592654;

        public frmGalaza()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Class constructor
            //------------------------------------------------------------------------------------------------------------------
            
            // Invoke
            FormClosed += frmGame_FormClosed;
            FormClosing += frmGame_FormClosing;
            Paint += form1_Paint;
            Load += frmGame_Load;

            // This call is required by the Windows Form Designer
            InitializeComponent();
        }

        private void frmGame_Load(object sender, System.EventArgs e)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Program entry point (using the constructor for the below gives strange problems!)
            //
            // 1. Init components
            // 2. Create screen buffer
            // 3. Enable double buffering
            // 4. Show game screen and set focus
            // 5. Init game ...
            // 6. Load high scores from disk
            // 7. Merge to main game loop
            //------------------------------------------------------------------------------------------------------------------

            // Create a new graphics buffer (this handles the entire screen)
            scrnBufferBmp = new Bitmap(ScreenWidth, ScreenHeight, this.CreateGraphics());
            graphicsBuffer = Graphics.FromImage(scrnBufferBmp);

            // Enable double buffering ensuring minimal flicker
            SetStyle(ControlStyles.DoubleBuffer, true);
            SetStyle(ControlStyles.AllPaintingInWmPaint, true);

            // Make the game visible 
            this.Show();
            this.Focus();

            // Init game
            initialize();

            // Load high scores from disk
            loadScoreTable();

            // Merge to main loop
            mainLoop();

        }

        private void mainLoop()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Game loop ...
            // 1. Game not being played: show game title ...
            // 2. Game being played then enable the invaders to do their thing
            // 3. Manage game frame rate (50fps)
            // 4. Manage time prescalers
            // 5. Fetch player input ...
            // 6. Refresh screen with new frame every 20mS (50fps)
            //------------------------------------------------------------------------------------------------------------------

            // Time taken to render a given frame (~10mS average on a Pentium 4 3.0GHz running Win XP)
            int mS = 0;

            // Play background music
            if (!sndEngineError)
            {
                bkMusic[0].playSND(true);
            }

            // Inifinite loop until the user terminates the game
            while (AppRunning)
            {
                //                       :: TIME PRESCALERS DERIVED FROM 20mS BASE DELAY ::

                // 400mS
                tScales.setTScaleC(tScales.getTAcaleC() + 1);
                tScales.setTScaleC(tScales.getTAcaleC() %20);

                // 200mS
                tScales.setTScaleB(tScales.getTAcaleB() + 1);
                tScales.setTScaleB(tScales.getTAcaleB() % 10);

                // 100mS
                tScales.setTScaleA(tScales.getTAcaleA() + 1);
                tScales.setTScaleA(tScales.getTAcaleA() % 5);

                // 1 second
                tScales.setTScaleD(tScales.getTAcaleD() + 1);
                tScales.setTScaleD(tScales.getTAcaleD() % 50);

                // Generic calls regardless if the game is being played or not
                renderBackground(25);
                doScroll();
                renderInfo();


                // Do title screen if game is over ...

                if (clsShared.gameOver == true)
                {
                    // Title screen mode showing high scores
                    doTitle();
                }
                else  //- ' Game in progress begin with start of game procedure
                {
                    if (doingSOGame)
                    {
                        doStartOfGame(); 
                    }
                    else  //- ' Do start of level routine
                    {
                        if (doingSOLevel== true)
                        {
                            doStartOfLevel();
                        }
                        else //- If level has just been completed then do end of level routine
                        {
                            if (levelCompleted == true)
                            {
                                if (!player.isDead())
                                {
                                    doEndOfLevel();
                                }
                            }
                            else //- Game is being played, bring on the action
                            {
                                doEnemies();
                            }
                        }
                    }

                    // Calls whilst the game is being played ...
                    doPlayer();
                    doParticles();
                    doPickups();
                    checkCollisions();
                }

                // Draw screen buffer to the actual screen (straight to the form in this case)
                this.Invalidate(new Rectangle(0, 0, ScreenWidth, ScreenHeight));

                // 20mS delay between frames which means that the game runs at 50fps
                mS = regulatedDelay(20);

                // Fetch player input from the keyboard
                fetchKeys();
            }

            // Loop exited, terminate app ... 
            this.Close();
        }

        private void doEnemies()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Handles all invader invaders. Calling all the shots by bringing the invader sprites to life.
            //
            // 1. Invaders are launched one at a time along a path (5 batches of 12 equating to 60 invaders each level)
            // 2. Invaders have the ability to atack the player whilst being in this path, they can shoot or dive
            // 3. Invaders are docked into a specific assigned cell after they have completed the launch path
            // 4. Invaders hover back and forth in their dock ...
            // 5. Invaders will randomly shoot or swoop from their dock cell to attack the player
            //
            // # Method is called every frame (20mS) of the game ...
            //   The game progressively becomes harder and harder with more invaders attacking in each succesive level.
            //------------------------------------------------------------------------------------------------------------------

            // Specific vars ...
            bool dispatchNew = false;

            // Flag set when it is time to dispatch a new segment of invaders in a path
            int speed = 0;

            // Variable path speed ...
            // Hover direction left or right when the invaders are docked  
            // Simple 2 sprite animation for the invaders

            // General working vars (multi purpose: loops, cache, counters etc ...)
            int i = 0;
            int j = 0;
            int k = 0;
            int e = 0;
            int c = 0;
            int t = 0;

            //                                ~:: LAUNCH INVADERS ONE AT A TIME ALONG PATHS ::~ 
            // Invaders: 4-18-2015
            if (tInvadersLaunched < 59)
            {
                for (i = invadersFinishedPath; i <= tInvadersInPath; i++)
                {
                    for (j = 0; j <= 1; j++)
                    {
                        switch (invaderEntryPath)
                        {

                            case 0:
                                //          :: Side of screen entry (3 iterations per level) ::

                                // Do not follow path if diving (invader swoops out of the chain if diving)

                                if (!invaders[(i + invaderBatchStartIndex) + (j * 6)].isDiving())
                                {
                                    // Left
                                    invaders[i + invaderBatchStartIndex].setX(paths.getLeftPointEntryX(73 - entryPathIndex[i]) - 50);
                                    invaders[i + invaderBatchStartIndex].setY(paths.getLeftPointEntryY(73 - entryPathIndex[i]) - 50);

                                    // Right 
                                    invaders[i + (invaderBatchStartIndex + 6)].setX(paths.getRightPointEntryX(entryPathIndex[i]) + 25);
                                    invaders[i + (invaderBatchStartIndex + 6)].setY(paths.getRightPointEntryY(entryPathIndex[i]) - 70);
                                }

                                // Set speed and bail
                                speed = 1;
                                break;
                            
                            case 1:
                                //          :: Center of screen entry (2 iterations per level) ::

                                // Do not follow path if diving (invader swoops out of the chain if diving)

                                if (!invaders[(i + invaderBatchStartIndex) + (j * 6)].isDiving())
                                {
                                    // Left  
                                    invaders[i + invaderBatchStartIndex].setX(paths.getLeftPointEntryX(entryPathIndex[i]) - 30);
                                    invaders[i + invaderBatchStartIndex].setY(paths.getLeftPointEntryY(entryPathIndex[i]) - 100);

                                    // Right   
                                    invaders[i + (invaderBatchStartIndex + 6)].setX(paths.getRightPointEntryX(73 - entryPathIndex[i]) + 5);
                                    invaders[i + (invaderBatchStartIndex + 6)].setY(paths.getRightPointEntryY(73 - entryPathIndex[i]) - 120);
                                }

                                // Set speed and bail
                                speed = 2;
                                break;
                        }

                        // Random attacking while in paths (more likely to occur with each succesive level)
                        e = getRandomNumber(0, 50 - level);  //was 100

                        // Ensure e never goes below zero
                        if (e < 0)
                            e = 0;

                        if (e == 0)
                        {
                                //                            :: Diving ::

                                // 1. Level denotes how many divers there can be at any given time
                                // 2. Apply only to invader which has not been docked
                                // 3. Ensure invader is on the screen by at least 50 pixels

                                if (tDiversInPath < (level + 2))
                                {
                                    if (!invaders[(i + invaderBatchStartIndex) + (j * 6)].hasDockingCell())
                                    {
                                        switch (j)
                                        {
                                            case 0:
                                                if (invaders[i + invaderBatchStartIndex].getX() > 75)
                                                {
                                                    findDockSpace(i + invaderBatchStartIndex, false);
                                                    invaders[i + invaderBatchStartIndex].setDoDiving(true, player.getX(), 1);
                                                    tDiversInPath += 1;
                                                }

                                                break;

                                            case 1:
                                                if (invaders[(i + invaderBatchStartIndex) + (j * 6)].getX() < 925)
                                                {
                                                    findDockSpace((i + invaderBatchStartIndex) + (j * 6), false);
                                                    invaders[(i + invaderBatchStartIndex) + (j * 6)].setDoDiving(true, player.getX(), 1);
                                                    tDiversInPath += 1;
                                                }

                                                break;
                                        }
                                    }
                                }
                            }
                            else if (e < 20)
                            {
                                //                     :: Shooting ::

                                // 1. The actual level denotes as to how many shooters there can be at a time
                                // 2. Ensure invader is not already shooting ...
                                // 3. Avoid bullets in the very outter region of the screen

                                if (tShootersInPath < (level + 12))
                                {
                                    if (!invaders[(i + invaderBatchStartIndex) + (j * 6)].isShooting())
                                    {
                                        switch (j)
                                        {
                                            case 0:
                                                if (invaders[i + invaderBatchStartIndex].getX() > getRandomNumber(50, 100))
                                                {
                                                    invaders[i + invaderBatchStartIndex].setDoShooting(true);
                                                    tShootersInPath += 1;
                                                }

                                                break;
                                            
                                            case 1:
                                                if (invaders[(i + invaderBatchStartIndex) + (j * 6)].getX() < getRandomNumber(500, 550))
                                                {
                                                    invaders[(i + invaderBatchStartIndex) + (j * 6)].setDoShooting(true);
                                                    tShootersInPath += 1;
                                                }

                                                break;
                                         }
                                    }
                                }
                            }
                        }

                    // Dock invader when the path has been completed 

                    if (entryPathIndex[i] >= 72)
                    {
                        // Inc total invaders launched / finished their path
                        invadersFinishedPath += 1;
                        tInvadersLaunched += 2;

                        // Locate a specific cell for the invader which they will retain throughout the entire level
                        for (j = 0; j <= 1; j++)
                        {
                            if (!invaders[(i + invaderBatchStartIndex) + (j * 6)].hasDockingCell())
                            {
                                findDockSpace((i + invaderBatchStartIndex) + (j * 6), true);
                            }
                        }
                    }
                    else //- Invader still in path, inc invader along their path ...
                    {
                        entryPathIndex[i] += speed;
                    }
                }

                //                                       ~:: RELEASE NEW INVADERS ::~

                // One new invader ...
                if (tInvadersInPath < 5)
                {
                    releaseNewInvader += 1;
                    if (releaseNewInvader == 13)
                    {
                        tInvadersInPath += 1;
                        releaseNewInvader = 0;
                    }
                }

                // Begin new batch of them when the current batch of them have finished the path ...
                if (invadersFinishedPath == 6)
                {
                    switch (tInvadersLaunched)
                    {
                        case 12:
                        case 24:
                        case 36:
                        case 48:
                            dispatchNew = true;
                            invaderBatchStartIndex = tInvadersLaunched;
                            break;
                    }
                }

                // Do not dispatch new lot if player has been shot, wait until player is active again
                if (!player.isDead())
                {
                    if (dispatchNew)
                    {
                        // Short delay between launching next batch
                        if (tScales.getTAcaleC() == 0)
                            newBatchDelay += 1;

                        if (newBatchDelay == 3)
                        {
                            // Toggle paths
                            invaderEntryPath += 1;
                            invaderEntryPath = invaderEntryPath % 2;

                            // Reset and prepare to repeat
                            for (i = 0; i <= 5; i++)
                            {
                                entryPathIndex[i] = 0;
                            }

                            // Reset
                            tInvadersInPath = 0;
                            invadersFinishedPath = 0;
                            newBatchDelay = 0;
                            dispatchNew = false;
                        }
                    }
                }
            }

            //                          ~:: SHIFT INVADERS BACK & FORTH IN THEIR DOCKING CELLS ::~ 

            // Check if cells have exceeded specified X-axis and toggle direction if so ...
            for (i = 0; i <= 59; i++)
            {
                if (dockActive[i])
                {
                    if (dockRect[i].X < 5)
                    {
                        static_doEnemies_dir = 0;
                    }
                    else if (dockRect[i].X > 960)
                    {
                        static_doEnemies_dir = 1;
                    }
                }
            }

            // Shift all cells ... 
            for (i = 0; i <= 59; i++)
            {
                switch (static_doEnemies_dir)
                {
                    case 0:
                        dockRect[i].X += 1;
                        break;
                    case 1:
                        dockRect[i].X -= 1;
                        break;
                }

                // Apply only to active invaders (invaders that are alive)
                if (invaders[i].isActive())
                {
                    // Invader must be docked not diving
                    if (invaders[i].isDocked())
                    {
                        invaders[i].setX(dockRect[invaders[i].getDockingCell()].X);
                        invaders[i].setY(dockRect[invaders[i].getDockingCell()].Y);

                        // Apply x axis offset to the larger of the 3 invaders ...
                        // this effectively centers it in the cell making it uniform with the others
                        switch (invaders[i].getInvaderType())
                        {
                            case 3:
                            case 4:
                            case 5:
                                invaders[i].setX(invaders[i].getX() - 5);
                                invaders[i].setY(invaders[i].getY() - 8);
                                break;
                        }
                    }
                }
            }

            //            ~:: FETCH INVADER ATTACK MOVES WHEN DOCKED -- RANDOM DIVES TOWARDS THE PLAYER & SHOOTING ::~ 

            // Do not proceed to attack if the player has just been killed

            if (!player.isDead())
            {
                // Fetch moves every 200mS
                if (tScales.getTAcaleC() == 0)
                {
                    // The odds of the invaders attacking increases with each new level 
                    j = getRandomNumber(0, level + 10);

                    // Random invader to apply moves to
                    i = getRandomNumber(0, 60);

                    // 50/50 chance of an invader attcking with the first level

                    if (j > 5)
                    {
                        // Tally up how many invaders are attacking ...
                        for (e = 0; e <= 59; e++)
                        {
                            if (invaders[e].isActive())
                            {
                                if (invaders[e].isShooting() | invaders[e].isDiving())
                                {
                                    t += 1;
                                }
                            }
                        }

                        // The number of invaders atacking can not exceed the level number + 2
                        if (t < (level + 2))
                        {
                            while (!(k == 1))
                            {
                                // Find an invader which is active and not attacking loop until we do
                                if (invaders[i].isActive())
                                {
                                    if (!invaders[i].isShooting())
                                    {
                                        k = 1;
                                        e = i;
                                    }
                                }

                                // Fetch random ...
                                i = getRandomNumber(0, 60);

                                // Fail safe counter
                                c += 1;

                                // Time out can't find invader not already shooting
                                if (c == 1000)
                                {
                                    break; 
                                }
                            }
                        }

                        // Found invader ...
                        if (k == 1)
                        {
                            // Invoke shoot method
                            invaders[e].setDoShooting(true);

                            // If docked and not diving then invoke dive method too
                            if (invaders[e].isDocked())
                            {
                                if (!invaders[e].isDiving())
                                {
                                    invaders[e].setDoDiving(true, player.getX(), 0);
                                    invaders[e].setDocked(false);
                                    invaders[e].setDoRotate(true);

                                    // Play snd effect
                                    if (!sndEngineError)
                                    {
                                       _clsCommon.sndEffects[0].playSND(false);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            //               ~:: CALLS TO INVADER ATTACKING & DOCKING METHODS + RENDER ALL ACTIVE INVADERS ::~

            // Loop through all ...
            for (i = 0; i <= 59; i++)
            {
                // Only interested in active invaders
                if (invaders[i].isActive())
                {
                    // Dock invader if set to do so (merge towards cell)
                    if (invaders[i].isDocking())
                    {
                        //                     :: Inc x axis ::

                        if (invaders[i].getX() < dockRect[invaders[i].getDockingCell()].X)
                        {
                            if (invaders[i].isRedocking())
                            {
                                invaders[i].setX(invaders[i].getX() + 2);
                            }
                            else
                            {
                                invaders[i].setX(invaders[i].getX() + 10);
                            }

                            //                 :: Dec x axis ::

                        }
                        else if (invaders[i].getX() > dockRect[invaders[i].getDockingCell()].X)
                        {
                            if (invaders[i].isRedocking())
                            {
                                invaders[i].setX(invaders[i].getX() - 2);
                            }
                            else
                            {
                                invaders[i].setX(invaders[i].getX() - 10);
                            }
                        }

                        //                     :: Dec y axis ::

                        if (invaders[i].getY() > dockRect[invaders[i].getDockingCell()].Y)
                        {
                            if (invaders[i].isRedocking())
                            {
                                invaders[i].setY(invaders[i].getY() - 2);
                            }
                            else
                            {
                                invaders[i].setY(invaders[i].getY() - 10);
                            }

                            //                 :: Inc y axis ::

                        }
                        else if (invaders[i].getY() < dockRect[invaders[i].getDockingCell()].Y)
                        {
                            if (invaders[i].isRedocking())
                            {
                                invaders[i].setY(invaders[i].getY() + 2);
                            }
                            else
                            {
                                invaders[i].setY(invaders[i].getY() + 10);
                            }
                        }

                        //      :: Snap the invader into position if it intersects with the dock cell ::

                        // Fetch collision rect specifics
                        rect1.X = invaders[i].getRectX();
                        rect1.Y = invaders[i].getRectY();
                        rect1.Width = invaders[i].getRectW();
                        rect1.Height = invaders[i].getRectW();

                        // Lock it into position if it intersects

                        if (dockRect[invaders[i].getDockingCell()].IntersectsWith(rect1))
                        {
                            // Apply x & y axis to invader
                            invaders[i].setY(dockRect[invaders[i].getDockingCell()].Y);
                            invaders[i].setX(dockRect[invaders[i].getDockingCell()].X);

                            // Offset for the larger invader
                            if (invaders[i].getInvaderType() == 2 | invaders[i].getInvaderType() == 3)
                            {
                                invaders[i].setX(invaders[i].getX() - 5);
                                invaders[i].setY(invaders[i].getY() - 8);
                            }

                            // Method completed
                            invaders[i].setDocked(true);
                            invaders[i].setDoDocking(false);

                            // If invader is re-docking after a dive then do a 360 rotate
                            if (invaders[i].isRedocking())
                            {
                                invaders[i].setRedocking(false);
                                invaders[i].setDoRotate(true);
                            }

                            // This dock is now active
                            dockActive[invaders[i].getDockingCell()] = true;
                        }
                    }

                    // Do dive method if set
                    if (invaders[i].isDiving())
                    {
                        invaders[i].doDive();
                    }

                    // Do rotate if set ...
                    if (invaders[i].isRotating())
                    {
                        invaders[i].setAngle(invaders[i].getAngle() + 0.1);
                        if (invaders[i].getAngle() > (2 * PI))
                        {
                            invaders[i].setDoRotate(false);
                            invaders[i].setAngle(0);
                        }
                    }

                    // Render invader sprites
                    invaders[i].Draw(graphicsBuffer, invaders[i].getAngle(), static_doEnemies_ani, invaders[i].isRotating());
                }

                // Do shoot method if set (invader bullets still appear on the screen after they have been shot)
                // this is why the calls to this method are outside of the above block
                if (invaders[i].isShooting())
                {
                    invaders[i].doShoot(graphicsBuffer);
                }
            }

            // Animate invaders (constant 2 sprite animation flicking between two different images)
            if (tScales.getTAcaleC() == 0)
                static_doEnemies_ani += 1;
                static_doEnemies_ani = static_doEnemies_ani % 2;

        }
        static int static_doEnemies_ani = 0;
        static int static_doEnemies_dir = 0;

        private void doPlayer()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Handles the player's ship
            //
            // 1. Move the ship left or right at player's request
            // 2. Fire bullets at player's request
            // 3. Deal with the player's ship being destroyed
            // 4. Calls to move bullets methods ...
            // 5. Animate & render player's sprite to the back buffer
            //------------------------------------------------------------------------------------------------------------------

            // General working vars
            int i = 0;
            int j = 0;
            int x = 0;
            int y = 0;
            string s = null;
            bool bailOut = false;

            if (!player.isDoingWarp())
            {
                // Fetch player coords
                x = player.getPivotX(playerPivotPos);
                y = player.getPivotY(playerPivotPos);

                // Apply player coords
                player.setY(y + 25);
                player.setX(x - 20);
                player.setWarpH(0);
                player.setWarpW(0);
            }
            else //- Do end of level special effect (warp / rocket player forwards to horizon)
            {

                if (player.getY() > 5)
                {
                    // Move player forwards 2 pixels p/frame
                    player.setY(player.getY() - 4);

                    // Scale sprite (making it smaller as it rockets forward ...
                    if (tScales.getTAcaleB() == 0)
                    {
                        player.setWarpH(player.getWarpH() + 1);
                        player.setWarpW(player.getWarpW() + 1);
                    }
                }

                // Allow player to move the ship as they rocket forward
                // fetch coords and apply ...
                x = player.getPivotX(playerPivotPos);
                player.setX(x - 20);
            }

            // Disallow shooting during end of level scenes otherwise shoot at player's request
            // and obvioulsy no shooting is allowed if the player is dead ...

            if (!player.isDead())
            {
                // Shift ship left or right at player's request
                if (moveKeyPress[0] && clsShared.AbductionPhase==0)
                {
                    if (playerPivotPos > 0)
                        playerPivotPos -= 1;
                }
                else if (moveKeyPress[1] && clsShared.AbductionPhase == 0)
                {
                    if (playerPivotPos < 121)
                        playerPivotPos += 1;
                }

                if (!levelCompleted)
                {
                    if (shootKeyPress)
                    {
                        // The firing rate is governed
                        if (delayNextShot == 0)
                        {
                            // Key must be released before another shot can be fired
                            if (shootKeyUP && clsShared.AbductionPhase==0 )
                            {
                                shootKeyUP = false;

                                // Create new instance of a player bullet
                                for (i = 0; i <= 5; i++)
                                {
                                    if (playerbullet[i, 0] == null)
                                    {
                                        switch (player.getFirePowerLevel())
                                        {
                                            case 1: //:: x1 fire ::

                                                // Create new instances
                                                playerbullet[i, 0] = new clsPlayerBullet(x - 2, y);
                                                if (player.DoubleWideShip)
                                                {
                                                    playerbullet[i, 1] = new clsPlayerBullet(x + 35, y + 10);
                                                }
                                                // Bail out
                                                bailOut = true;
                                                break;

                                            case 3: //:: x3 fire ::
                                               
                                                // Create new instances
                                                playerbullet[i, 0] = new clsPlayerBullet(x - 2, y);
                                                playerbullet[i, 1] = new clsPlayerBullet(x + 10, y + 10);
                                                playerbullet[i, 2] = new clsPlayerBullet(x - 15, y + 10);

                                                if (player.DoubleWideShip)
                                                {
                                                    playerbullet[i, 3] = new clsPlayerBullet(x +38, y);
                                                    playerbullet[i, 4] = new clsPlayerBullet(x +35, y + 10);
                                                    playerbullet[i, 5] = new clsPlayerBullet(x +40, y + 15);


                                                }
                                                // Dec num of fire round remaining
                                                player.setX3FireAmmo(player.getX3FireAmmo() - 1);

                                                // Bail out
                                                bailOut = true;
                                                break;

                                            case 5: //:: x5 fire ::

                                                // Create new instances
                                                playerbullet[i, 0] = new clsPlayerBullet(x - 2, y);
                                                playerbullet[i, 1] = new clsPlayerBullet(x + 10, y + 10);
                                                playerbullet[i, 2] = new clsPlayerBullet(x - 15, y + 10);
                                                playerbullet[i, 3] = new clsPlayerBullet(x + 20, y + 20);
                                                playerbullet[i, 4] = new clsPlayerBullet(x - 25, y + 20);
                                                playerbullet[i, 5] = new clsPlayerBullet(x + 25, y + 22);
                                                playerbullet[i, 6] = new clsPlayerBullet(x - 30, y + 22);
                                                if (player.DoubleWideShip)
                                                {
                                                    playerbullet[i, 7] = new clsPlayerBullet(x - 2+35, y);
                                                    playerbullet[i, 8] = new clsPlayerBullet(x + 10 + 35, y + 13);
                                                    playerbullet[i, 9] = new clsPlayerBullet(x - 15 + 35, y + 12);
                                                    playerbullet[i, 10] = new clsPlayerBullet(x + 20 + 35, y + 11);
                                                    playerbullet[i, 11] = new clsPlayerBullet(x - 25 + 35, y + 10);
                                                    playerbullet[i, 12] = new clsPlayerBullet(x + 25 + 35, y + 9);
                                                    playerbullet[i, 13] = new clsPlayerBullet(x - 30 + 35, y + 7);
                                                
                                                }
                                                // Dec num of fire round remaining
                                                player.setX5FireAmmo(player.getX5FireAmmo() - 1);

                                                // Bail out
                                                bailOut = true;
                                                break;                                                 
                                        }

                                        if (bailOut == true)
                                        {
                                            break;
                                        }
                                    }
                                }

                                // Play snd effect
                                if (!sndEngineError)
                                {
                                    _clsCommon.sndEffects[1].playSND(false);
                                }

                                // Flag off and set governing delay (~5 round p/sec)
                                shootKeyPress = false;
                                delayNextShot = 12;

                            }
                        }
                    }
                }

                // Dec governeing delay
                if (delayNextShot > 0)
                    delayNextShot -= 1;

                // Key flags off
                moveKeyPress[0] = false;
                moveKeyPress[1] = false;

            }
            else //- The player has been shot or an invader has crashed into them
            {
                // Duration of the ship showing explosion graphics
                genWorkingCounter += 1;

                if (genWorkingCounter < 200)
                {
                    // Set do explosion flag ...
                    playerExplosion = true;
                }
                else if (genWorkingCounter < 350)
                {
                    // Place the new ship in the center of the screen
                    playerPivotPos = 61;

                    // Explosion flag off
                    playerExplosion = false;

                    // Render star graphic
                    //                    graphicsBuffer.DrawImage(star, new Rectangle(27, 265, star.Width, star.Height - 50), 0, 0, star.Width, star.Height, GraphicsUnit.Pixel, ImageAtt);

                    if (clsShared.lives != -1)
                    {

                        if (!levelCompleted)
                        {
                            // Set string
                            s = "GET READY";

                            // Flash player's new ship for a few seconds before it becomes active again
                            if (tScales.getTAcaleC() == 0)
                                enPlayerSprite += 1;
                            enPlayerSprite = enPlayerSprite % 2;

                        }
                        else // Force to case else and subtract a life and start the next level ...
                        {
                            genWorkingCounter = 350;
                        }
                    }
                    else //- Game over - //
                    {
                        s = "GAME OVER!";
                        enPlayerSprite = 1;
                        //Abduction complete
                        _clsCommon.sndEffects[6].stopSND();
                        _clsCommon.sndEffects[7].stopSND();
                        clsShared.global_gravity_warp_on = false;
                        clsShared._clsPlayer.setWarpY(0);
                        clsShared.Global_Abducted = false;
                        clsShared.AbductionPhase = 0;

 
                    }

                    // Show text string
                    flashText(s, 359, 424, 25);
                }
                else
                {
                    if (clsShared.lives == -1)
                    {
                        clsShared.gameOver = true;
                        CheckForNewHighScore();
                    }
                    else
                    {
                        clsShared.lives -= 1;
                    }

                    // Player is no longer dead
                    player.setPlayerDead(false);

                    // Reset vars
                    genWorkingCounter = 0;
                    enPlayerSprite = 0;
                
                }               
            }

            // Calls to move bullets method if the sprite intance of a bullet exists
            for (i = 0; i <= 5; i++)
            {
                for (j = 0; j <= 6; j++)
                {
                    if (playerbullet[i, j] != null)
                    {
                        playerbullet[i, j].moveBullets(graphicsBuffer);
                        if (playerbullet[i, j].getY() <= 0)
                        {
                            playerbullet[i, j] = null;
                        }
                    }
                }
            }

            // Render sprite to canvas (back buffer)
            player.Draw(graphicsBuffer, playerExplosion, enPlayerSprite);
        }

        private void checkCollisions()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Collision checking ...
            //
            // 1. If invader is diving: check for a collision between the player's ship and the invader ...
            // 2. If the invader is shooting: check for a collision between their bullets and the player's ship
            // 3. If the player is shooting: check for collisions between the player's bullets and the invader
            // 4. If the player has a sheild then they are protected against invader fire and collisions
            // 5. Tall up the total num of invaders still remaining on the screen ...
            // 6. Award points for shooting invaders
            //------------------------------------------------------------------------------------------------------------------

            // General working vars
            int i = 0;
            int j = 0;
            int c = 0;

            // Specific vars ...
            int totalShot = 0;
            bool collision = false;
            bool invaderDead = false;

            if (!levelCompleted)
            {
                // Loop through all invader instances and check for collisions
                for (j = 0; j <= 59; j++)
                {
                    // Void checking if player is dead ...
                    if (!player.isDead())
                    {
                        // :: Check if invader bullets have intersected with the player's ship ::
                        if (invaders[j].isShooting())
                        {
                            // Enemy's bullets
                            rect1.X = invaders[j].getBulletRectX();
                            rect1.Y = invaders[j].getBulletRectY();
                            rect1.Width = invaders[j].getBulletRectW();
                            rect1.Height = invaders[j].getBulletRectH();

                            // Player's ship
                            rect2.X = player.getRectX();
                            rect2.Y = player.getRectY();
                            rect2.Width = player.getRectW();
                            rect2.Height = player.getRectH();

                            // Collision? set flag and cease shooting if so
                            if (rect1.IntersectsWith(rect2))
                            {
                                collision = true;
                                invaders[j].setDoShooting(false);
                            }

                            // Player have a sheild?
                            if (player.hasSheild())
                            {
                                // Player's sheild
                                rect2.X = player.getSheildRectX();
                                rect2.Y = player.getSheildRectY();
                                rect2.Width = player.getSheildRectW();
                                rect2.Height = player.getSheildRectH();

                                if (rect1.IntersectsWith(rect2))
                                {
                                    particle[j] = new clsParticle(invaders[j].getBulletRectX(), invaders[j].getBulletRectY(), invaders[j].getBulletRectW(), invaders[j].getBulletRectH());
                                    invaders[j].setDoShooting(false);
                                }
                            }
                        }

                        // :: Check for collisions between the invaders ship and the player's ship ::

                        if (invaders[j].isActive())
                        {
                            if (invaders[j].isDiving())
                            {
                                // Enemy ship 
                                rect1.X = invaders[j].getRectX();
                                rect1.Y = invaders[j].getRectY();
                                rect1.Width = invaders[j].getRectW();
                                rect1.Height = invaders[j].getRectH();

                                // Player's ship
                                rect2.X = player.getRectX();
                                rect2.Y = player.getRectY();
                                rect2.Width = player.getRectW();
                                rect2.Height = player.getRectH();

                                //  Collision? set flag if so ... 
                                //  invader dies along with the player, both player and invader are now inactive
                                if (rect1.IntersectsWith(rect2))
                                {
                                    collision = true;
                                    dockActive[invaders[j].getDockingCell()] = false;
                                    invaders[j].setActive(false);
                                }

                                // Player have a sheild?

                                if (player.hasSheild())
                                {
                                    // Player's sheild
                                    rect2.X = player.getSheildRectX();
                                    rect2.Y = player.getSheildRectY();
                                    rect2.Width = player.getSheildRectW();
                                    rect2.Height = player.getSheildRectH();

                                    if (rect1.IntersectsWith(rect2))
                                    {
                                        particle[j] = new clsParticle(invaders[j].getX(), invaders[j].getY(), invaders[j].getW(), invaders[j].getH());
                                        dockActive[invaders[j].getDockingCell()] = false;
                                        invaders[j].setActive(false);
                                    }
                                }
                            }
                        }

                        // Collision flag set? play sound effect and set player dead
                        if (collision && clsShared.AbductionPhase == 0)
                        {
                            if (!player.DoubleWideShip)
                            {
                                player.setPlayerDead(true);
                                //Abduction complete
                                clsShared.global_gravity_warp_on = false;
                                clsShared._clsPlayer.setWarpY(0);
                                clsShared.Global_Abducted = false;
                                clsShared.AbductionPhase = 0;
                            
                            }

                            if (player.DoubleWideShip)
                            {
                                //Player reverts back to single size ship (and does not die)
                                player.DoubleWideShip = false;
                            }

                            // Play snd effect
                            if (true)
                            {
                                _clsCommon.sndEffects[6].playSND(false);

                         
                                _clsCommon.sndEffects[3].playSND(false);

                            }
                        }
                    }

                    //:: Check for collisions between the player's bullets and the invaders ::

                    for (i = 0; i <= 5; i++)
                    {
                        for (c = 0; c <= 6; c++)
                        {
                            // Bullet instance must exist ...
                            if (playerbullet[i, c] != null)
                            {
                                // Invader must be still active
                                if (invaders[j].isActive())
                                {
                                    // 1. Player's bullet(s)
                                    rect1.X = playerbullet[i, c].getRectX();
                                    rect1.Y = playerbullet[i, c].getRectY();
                                    rect1.Width = playerbullet[i, c].getRectW();
                                    rect1.Height = playerbullet[i, c].getRectH();

                                    // 2. Enemy rect ...
                                    rect2.X = invaders[j].getRectX();
                                    rect2.Y = invaders[j].getRectY();
                                    rect2.Width = invaders[j].getRectW();
                                    rect2.Height = invaders[j].getRectH();

                                    // Collision?
                                    if (rect1.IntersectsWith(rect2))
                                    {
                                        // Create new particle instance
                                        particle[j] = new clsParticle(invaders[j].getX(), invaders[j].getY(), invaders[j].getW(), invaders[j].getH());

                                        // Different invaders have different degrees of points awarded ...
                                        // and some invaders require 2 or 3 shots before they are killed
                                        switch (invaders[j].getInvaderType())
                                        {


                                            case 0: //:: Blue invader 1 shot kills ::

                                                // Inc score and set flag
                                                score += 50;
                                                invaderDead = true;




                                                if (invaders[j].ship_abducted)
                                                {
                                                    //Return Ship to player
                                                    clsShared.lives += 1;
                                                    player.DoubleWideShip = true;
                                                    clsShared.Global_Abducted = false;

                                                    invaders[j].ship_abducted = false;
                                                    //Play sound since Player regained ship
                                                    _clsCommon.sndEffects[6].playSND(false); //WARP SOUND

                                                }



                                                break;

                                            case 1: //:: Red invader requires 2 shots to kill ::
                                                
                                                // Change colour to yellow
                                                invaders[j].setInvaderType(2);
                                                break;

                                            case 2: //:: Yellow invader, was prev red 2nd shot kills ::

                                                // Inc score and set flag
                                                invaderDead = true;
                                                score += 100;
                                                break;

                                            case 3: //:: Large green invader takes 3 shots to kill ::

                                                // Swap colour
                                                invaders[j].setInvaderType(4);
                                                break;

                                            case 4: //:: Large invader 2nd shot ::
                                                
                                                // Swap colour again
                                                invaders[j].setInvaderType(5);
                                                break;

                                            case 5: //:: Large invader final shot ::

                                                // Inc score and set flag
                                                invaderDead = true;
                                                score += 250;



                                                if (invaders[j].ship_abducted)
                                                {
                                                    //Return ship to player
                                                    clsShared.Global_Abducted = false;


                                                    clsShared.lives += 1;
                                                    player.DoubleWideShip = true;
                                                    invaders[j].ship_abducted = false;
                                                    _clsCommon.sndEffects[6].playSND(false); //WARP SOUND

                                                }
                                            
                                            // Drop pickup for killing this invader

                                                if (pickup == null)
                                                {
                                                    // Fetch random pickup to drop
                                                    bool voidPickup = false;
                                                    c = getRandomNumber(0, 3);

                                                    switch (c)
                                                    {
                                                        case 0: //:: x3 firepower ::
                                                            
                                                            // Void if player alrewady has it
                                                            if (player.getFirePowerLevel() == 3)
                                                            {
                                                                voidPickup = true;
                                                            }

                                                            break;

                                                        case 1: //:: Sheild ::
                                                            
                                                            // Void if player already has it
                                                            if (player.hasSheild())
                                                            {
                                                                voidPickup = true;
                                                            }

                                                            break;

                                                        case 2: //:: x5 firepower ::
                                                            
                                                            // Void if player already has it
                                                            if (player.getFirePowerLevel() == 5)
                                                            {
                                                                voidPickup = true;
                                                            }
                                                            break;
                                                    }

                                                    // Create new instance of pickup to scroll
                                                    if (!voidPickup)
                                                    {
                                                        pickup = new clsPickups(invaders[j].getX(), invaders[j].getY(), c);
                                                    }
                                                }
                                                break;
                                        }

                                        // Enemy flag set? invader is now inactive
                                        if (invaderDead)
                                        {
                                            dockActive[invaders[j].getDockingCell()] = false;
                                            invaders[j].setActive(false);
                                            invaderDead = false;

                                            // Play snd effect
                                            if (!sndEngineError)
                                            {
                                                _clsCommon.sndEffects[2].playSND(false);
                                            }
                                        }

                                        // Dispose of player bullet instance after collision
                                        playerbullet[i, c] = null;
                                    }
                                }
                            }
                        }
                    }

                    // Tally up how many invaders are still active on the screen
                    if (!invaders[j].isActive())
                    {
                        totalShot += 1;
                    }

                    // When the entire 60 are shot the level has been completed ...
                    if (totalShot == 60)
                    {
                        levelCompleted = true;

                        // Sheild off
                        player.setSheild(false);

                        //bail
                        return;
                    }
                }
            }
        }

        private void doPickups()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Deal with pickup weapons, scroll them down the screen and detect when the player has got it and apply it
            //
            // 1. Sheild which last for 10 secs ...
            // 2. X3 fire which gives the player 20 shots
            // 3. X5 fire which gives the player 15 shots
            //------------------------------------------------------------------------------------------------------------------

            // The player can hold a sheild for 10 secs before they lose it
            if (player.hasSheild())
            {
                player.setSheildTime(player.getSheildTime() + 1);
                if (player.getSheildTime() > 500)
                {
                    player.setSheild(false);
                }
            }

            // x3 fire (20 shots)
            if (player.getFirePowerLevel() == 3)
            {
                if (player.getX3FireAmmo() <= 0)
                {
                    player.setFirePower(1);
                }
            }

            // x5 fire (15 shots)
            if (player.getFirePowerLevel() == 5)
            {
                if (player.getX5FireAmmo() <= 0)
                {
                    player.setFirePower(1);
                }
            }

            if (!levelCompleted)
            {
                if (pickup != null)
                {
                    // Pickup ...
                    rect1.X = pickup.getRectX();
                    rect1.Y = pickup.getRectY();
                    rect1.Width = pickup.getRectW();
                    rect1.Height = pickup.getRectH();

                    // Player's ship
                    rect2.X = player.getRectX();
                    rect2.Y = player.getRectY();
                    rect2.Width = player.getRectW();
                    rect2.Height = player.getRectH();

                    if (pickup.getY() > ScreenHeight)
                    {
                        pickup = null;
                    }
                    else
                    {
                        pickup.movePickups(graphicsBuffer);
                    }

                    // Collision with pickup? assign the player with it and kill off instance
                    if (rect1.IntersectsWith(rect2))
                    {
                        switch (pickup.getPickupType())
                        {
                            case 0: // x3 power
                                player.setFirePower(3);
                                player.setX3FireAmmo(200);
                                break;

                            case 1: // Sheild
                                player.setSheild(true);
                                player.setSheildTime(0);
                                break;

                            case 2: // x5 power
                                player.setFirePower(5);
                                player.setX5FireAmmo(50);
                                break;
                        }

                        // Dispose
                        pickup = null;

                        // Play snd effect
                        if (!sndEngineError)
                        {
                            _clsCommon.sndEffects[4].playSND(false);
                        }
                    }
                }
            }
            else // Kill instance
            {
                pickup = null;
            }
        }

        private void initNewInvaders(bool newGame)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Random new invaders
            //
            // 1. Reset all class properties of the invader class
            // 2. Fetch new types ...
            // 3. Null numerous variables ready for a new level or fresh game
            //------------------------------------------------------------------------------------------------------------------
            // NEW INVADERS
            int i = 0;
            int j = 0;
            int k = 0;
            int e = 0;
            int b = 0;
            int t = 0;

            // 60 new invaders (6 x 10)
            for (j = 0; j <= 5; j++)
            {
                for (i = 0; i <= 9; i++)
                {
                    // 6 invaders p/ path
                    if (b >= 6)
                    {
                        b = 0;
                        e = getRandomNumber(0, 2);
                        t = e;
                    }

                    // Pickup rewarding large green invader (12 p/level)
                    switch (k)
                    {
                        case 3:
                        case 9:
                        case 12:
                        case 18:
                        case 21:
                        case 27:
                        case 30:
                        case 36:
                        case 39:
                        case 45:
                        case 48:
                        case 54:
                            e = 3;
                            break;
                        default:
                            e = t;
                            break;
                    }

                    // Reset specifics and set active
                    invaders[k].resetAll();
                    invaders[k].setActive(true);
                    invaders[k].setInvaderType(e);

                    // Dock flags off
                    dockReserved[k] = false;
                    dockActive[k] = false;

                    // Dock collision rects reset
                    dockRect[k].X = (i * 50) + 75; //Halford
                    dockRect[k].Y = (j * 40) + 55;
                    dockRect[k].Width = 50;
                    dockRect[k].Height = 50;

                    // Inc gen working counters
                    k += 1;
                    b += 1;
                }

                // Reset path counter
                entryPathIndex[j] = 0;
            }

            // Null vars for new level 
            tInvadersLaunched = 0;
            releaseNewInvader = 0;
            invadersFinishedPath = 0;
            invaderBatchStartIndex = 0;
            invaderEntryPath = 0;
            newBatchDelay = 0;
            tInvadersInPath = 0;
            tDiversInPath = 0;
            tShootersInPath = 0;
            levelCompleted = false;

            // Null vars for new game ...
            if (newGame)
            {
                level = 1;
                clsShared.lives = 2;
                playerPivotPos = 61;
                player.setPlayerDead(false);
                playerExplosion = false;
                enPlayerSprite = 0;
                player.setFirePower(1);
                player.setSheild(false);
                score = 0;
            }
        }

        private void fetchKeys()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: retrieve player input from keyboard
            //------------------------------------------------------------------------------------------------------------------

            // Left & right
            if (GetAsyncKeyState(39) != 0)
            {
                moveKeyPress[0] = true;
            }
            else if (GetAsyncKeyState(37) != 0)
            {
                moveKeyPress[1] = true;
            }

            // Fire  (CTRL or SPACEBAR) (17=ctrl,32=space)
            if (GetAsyncKeyState(32) != 0)
            {
                //if (shootKeyUP)
                    shootKeyPress = true;
            }
            else if (GetAsyncKeyState(32) == 0)
            {
                shootKeyUP = true;
            }

            // F2 key to Start new game
            if (GetAsyncKeyState(113) != 0)
            {
                clsShared.gameOver = false;
                doingSOGame = true;
                genWorkingCounter = 0;
                SoundEnabled = true;
                InitSound();
                    
            }

            if (GetAsyncKeyState(114) != 0)
            {
                clsShared.gameOver = false;
                doingSOGame = true;
                genWorkingCounter = 0;
                SoundEnabled = false;

                InitSound();
                    
            }

        }

        private void doScroll()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Vector scrolling (just a series of lines running at different speeds which gives the perception of:
            //
            // a. Distance
            // b. Movement ...
            // An absolute complete illusion
            //------------------------------------------------------------------------------------------------------------------

            int speed = 0;

            // Apply scrolling speed
            if (player.isDoingWarp())
            {
                speed = 1; // Fast
            }
            else
            {
                speed = 3; // Slow
            }

            // Inc counter
            static_doScroll_timeScale += 1;

            // Scroll lines when counter equals or exceeds specified speed
            if (false && static_doScroll_timeScale >= speed)
            {
                for (int i = 0; i <= 7; i++)
                {
                    vectorScroll[i].setY(vectorScroll[i].getY() + vectorScroll[i].getIncAmount());
                    if (vectorScroll[i].getY() > vectorScroll[i].getFinishCycleY())
                    {
                        vectorScroll[i].setY(vectorScroll[i].getStartingY() + vectorScroll[i].getIncAmount());
                    }
                }

                // Reset
                static_doScroll_timeScale = 0;
            }

            // Render 
            for (int i = 0; i <= 7; i++)
            {
                if (false)
                {
                    vectorScroll[i].Draw(graphicsBuffer);
                }
            }

        }
        static int static_doScroll_timeScale = 0;

        private void doParticles()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Calls to render all active particles
            //------------------------------------------------------------------------------------------------------------------

            // Loop through all particle instances
            for (int i = 0; i <= 59; i++)
            {

                if (particle[i] != null)
                {
                    // Render if alive ...
                    particle[i].draw(graphicsBuffer);

                    // Kill intance if no longer active
                    if (!particle[i].isActive())
                    {
                        particle[i] = null;
                    }
                }
            }
        }

        private void fetchNewStars()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Create random new instances of the star class with random colors and locations 
            //------------------------------------------------------------------------------------------------------------------
            //StarField

            // Render star feild
            int j = 0;
            int x = 0;
            int y = 0;

            System.Drawing.Color starcol = default(System.Drawing.Color);
            restartStars:

            // Fetch 100 random star
            if (stars[0] != null)
            {
              //Make star field move down the screen
                for (int i = 0; i <= 99; i++)
                {
                    clsStars oldStars = stars[i];
                    oldStars.setY(oldStars.getY() + 3);
                    int Rnd1 = getRandomNumber(0, 2000);
                    if (Rnd1 == 1)
                    {
                        stars[0] = null;
                        goto restartStars;
                    }
                }
           


            }
            else
            {
                for (int i = 0; i <= 99; i++)
                {
                    // Colour
                    j = getRandomNumber(0, 3);

                    // X & Y locations
                    x = getRandomNumber(0, ScreenWidth);
                    y = getRandomNumber(0, ScreenHeight);

                    // Apply colour
                    switch (j)
                    {
                        case 0:
                            starcol = Color.Red;
                            break;
                        case 1:
                            starcol = Color.Green;
                            break;
                        case 2:
                            starcol = Color.Yellow;
                            break;
                    }

                    // Create class instance
                    stars[i] = new clsStars(x, y + 25, starcol);
                }
            }
        }

        private void findDockSpace(int invaderIndex, bool doDocking)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Locate a specific random dock cell for the invader
            //------------------------------------------------------------------------------------------------------------------

            int k = 0;

            while (dockReserved[k] == true)
            {
               k = getRandomNumber(0, 60);
            }

            if (invaders[invaderIndex].isActive())
            {
                invaders[invaderIndex].setDockingCell(k);
                invaders[invaderIndex].setDoDocking(doDocking);
                dockReserved[k] = true;
            }
        }

        private void createInvaderInstances()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Create instances of the invader class
            //------------------------------------------------------------------------------------------------------------------

            for (int i = 0; i <= 59; i++)
            {
                invaders[i] = new clsInvader(-50, 0, 0, true,this._clsCommon);
            }
        }

        private void DrawText(Graphics Destination, string Text, int x, int y, int FontSize, FontStyle Style, Brush Col)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Draw text to graphics buffer using specified attributes and text string
            //------------------------------------------------------------------------------------------------------------------

            // Font and specified attributes
            Font DrawFont = new Font("Verdana", FontSize, Style);
            StringFormat DrawFormat = new StringFormat();
            DrawFormat.FormatFlags = StringFormatFlags.NoFontFallback;

            // Draw the string to graphics buffer
            Destination.DrawString(Text, DrawFont, Col, x, y, DrawFormat);
        }

        private void renderBackground(int glowSpeed)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Render background bitmap (scwidth00xScrHeight)
            //------------------------------------------------------------------------------------------------------------------

            int i = 0;

            // Inc counter
            static_renderBackground_timeScale += 1;

            // Animate ...
            if (static_renderBackground_timeScale == glowSpeed)
            {
                static_renderBackground_timeScale = 0;

                // Switch neo glow frames
                if (static_renderBackground_dir == 0)
                {
                    static_renderBackground_ani += 1;
                }
                else
                {
                    static_renderBackground_ani -= 1;
                }
            }

            // Render static background ...
            graphicsBuffer.DrawImage(background[0], new Rectangle(0, 0, ScreenWidth, ScreenHeight), 0, 0, ScreenWidth, ScreenHeight, GraphicsUnit.Pixel, ImageAtt);

            // Render animated top portion with animated neon glow
 //            graphicsBuffer.DrawImage(background[static_renderBackground_ani + 1], new Rectangle(0, 100, ScreenWidth, 100), 0, 0, ScreenWidth, 100, GraphicsUnit.Pixel, ImageAtt);

            // Toggle back / forth when last frame is reached
            switch (static_renderBackground_ani)
            {
                case 4:
                    static_renderBackground_dir = 1;
                    break;
                case 0:
                    static_renderBackground_dir = 0;
                    break;
            }

            // Call to render all current stars
            for (i = 0; i <= 99; i++)
            {
                stars[i].Draw(graphicsBuffer);
            }

            // Find a new scattered batch at random
            if (tScales.getTAcaleC() == 0)
                fetchNewStars();
        }

        static int static_renderBackground_dir = 0;
        static int static_renderBackground_ani = 0;
        static int static_renderBackground_timeScale = 0;

        private void initDelay()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Set 1uS resolution for timegettime  
            //------------------------------------------------------------------------------------------------------------------

            timeBeginPeriod(1);
        }

        private int regulatedDelay(int mS)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Provides a precision, regulated delay: 1mS resolution
            //------------------------------------------------------------------------------------------------------------------

            int StartmS = 0;
            int ElapsedmS = 0;

            // Sample of time in mS
            StartmS = timeGetTime();
            clsShared.iAbductionCounter++;
            if (clsShared.AbductionPhase != 0)
            {
                if (clsShared.iAbductionCounter > 500)
                {
                    //Reset Abductor
                    clsShared.global_gravity_warp_on = false;
                    clsShared._clsPlayer.setWarpY(0);
                    clsShared.Global_Abducted = false;
                    clsShared.AbductionPhase = 0;
                 }
            }
            if (processStartmS != 0)
            {
                // Subtract the time that the frame has taken from the actual desired delay
                mS = mS - (StartmS - processStartmS);

                // Yeild to OS until specified delay has elapsed
                while ((ElapsedmS < mS))
                {
                    ElapsedmS = (timeGetTime() - StartmS);
                    Application.DoEvents();
                }
            }

            // Sample and bail
            processStartmS = timeGetTime();
            
            return mS;
        }

        private void renderInfo()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Render game info
            //
            // 1. Score ...
            // 2. High score
            // 3. Lives remaining
            // 4. Level ...
            // 5. Fire power x1, x3 or x5
            // 6. Sheild: yes / no with time remaining
            //------------------------------------------------------------------------------------------------------------------

            int i = 0;
            int j = 0;
            string s = null;

            // Lives ...
            DrawText(graphicsBuffer, "LIVES: ", 12, 800, 16, FontStyle.Bold, Brushes.Black);
            DrawText(graphicsBuffer, "LIVES: ", 10, 801, 16, FontStyle.Bold, Brushes.White);

            SolidBrush brush = new SolidBrush(Color.Green);
            Pen pen = new System.Drawing.Pen(Color.Black);

            // Draw bar graph depicting num of lives remaining
            for (i = 0; i <= clsShared.lives; i++)
            {
                graphicsBuffer.FillRectangle(brush, 95 + (i * 25), 805, 20, 16);
                graphicsBuffer.DrawRectangle(pen, 95 + (i * 25), 805, 20, 16);
            }

            // Clean up
            brush.Dispose();
            pen.Dispose();

            // Level
            DrawText(graphicsBuffer, "LEVEL: " + Convert.ToString(level),(int)(ScreenWidth*.5), 832, 16, FontStyle.Bold, Brushes.Black);
            DrawText(graphicsBuffer, "LEVEL: " + Convert.ToString(level), (int)(ScreenWidth*.5), 831, 16, FontStyle.Bold, Brushes.White);

            // Score + high score
            DrawText(graphicsBuffer, "SCORE: " + Convert.ToString(score), 10, 10, 16, FontStyle.Bold, Brushes.White);
            DrawText(graphicsBuffer, "HIGH SCORE: " + Convert.ToString(highScore), (int)(ScreenWidth*.4), 10, 16, FontStyle.Bold, Brushes.Red);

            // Firepower
            switch (player.getFirePowerLevel())
            {
                case 1:
                    s = "x1 (Unlimited Shots)";
                    break;
                case 3:
                    s = "x3 (" + player.getX3FireAmmo().ToString() + " Shots Remaining)";
                    break;
                case 5:
                    s = "x5 (" + player.getX5FireAmmo().ToString() + " Shots Remaining)";
                    break;
            }

            DrawText(graphicsBuffer, "Firepower: " + s, 12, 832, 14, FontStyle.Regular, Brushes.Black);
            DrawText(graphicsBuffer, "Firepower: " + s, 10, 831, 14, FontStyle.Regular, Brushes.White);

            // Sheild 
            switch (player.hasSheild())
            {
                case true:
                    i = player.getSheildTime();
                    j = Convert.ToInt32(10 - (i / 50));
                    s = j.ToString() + " Sec Remaining";

                    break;
                case false:
                    s = "Not Acquired";
                    break;
            }

            // Render
            DrawText(graphicsBuffer, "Shield: " + " " + s, 766, 832, 14, FontStyle.Regular, Brushes.Black);
            DrawText(graphicsBuffer, "Shield: " + " " + s, 768, 831, 14, FontStyle.Regular, Brushes.White);

        }

        private void CheckForNewHighScore()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Check to see if player's score has beaten any of the top 5 scores, prompt player for their name if so
            //------------------------------------------------------------------------------------------------------------------

            int i = 0;
            int j = 0;

            for (i = 0; i <= 4; i++)
            {
                if (score > Convert.ToInt32(Top5PlayerScore[i]))
                {
                    // Prompt player for their name

                    //if (frmNameEntry.ShowDialog() == Windows.Forms.DialogResult.OK)
                    //{
                        // Sort table
                        for (j = 4; j >= (i + 1); j += -1)
                        {
                            Top5PlayerName[i] = Top5PlayerName[j - 1];
                            Top5PlayerScore[j] = Top5PlayerScore[j - 1];
                        }

                        // Add new player
                        if (playersName == null)
                        {
                            playersName = "Anonymous";
                        }

                        Top5PlayerName[i] = playersName;
                        Top5PlayerScore[i] = score.ToString();

                        return;
                    //}
                }
            }
        }

        private void loadScoreTable()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Load saved high score from file, assign default values if no high scores exist
            //------------------------------------------------------------------------------------------------------------------

            string FILENAME = (Path.GetDirectoryName(Application.ExecutablePath) + "\\high scores.txt");

            // Get a StreamReader class that can be used to read the file  
            StreamReader objStreamReader = null;

            try
            {
                // Open file
                objStreamReader = File.OpenText(FILENAME);

                // Read player names & scores from file
                for (int i = 0; i <= 4; i++)
                {
                    Top5PlayerName[i] = objStreamReader.ReadLine();
                    Top5PlayerScore[i] = objStreamReader.ReadLine();
                }

                // Close the stream  
                objStreamReader.Close();
            }
            catch
            {
                // Load defaults, file dosen't exist 
                for (int i = 0; i <= 4; i++)
                {
                    Top5PlayerName[i] = "Gridcoin";
                }

                for (int i = 0; i <= 4; i++)
                {
                    int j;
                    j = 500000 - (i * 100000);
                    Top5PlayerScore[i] = (j.ToString());
                }

            }

            try
            {
                highScore = Convert.ToInt32(Top5PlayerScore[0]);
            }
            catch
            {
            }
        }

        private void SaveHighScores()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Save high scores and player's names to disk
            //------------------------------------------------------------------------------------------------------------------

            string FILENAME = (Path.GetDirectoryName(Application.ExecutablePath) + "\\high scores.txt");

            // Get a StreamReader class that can be used to read the file  
            FileStream objFileStream = null;
            objFileStream = File.Create(FILENAME);

            System.Text.ASCIIEncoding encoder = new System.Text.ASCIIEncoding();
            string Str = null;
            byte[] buffer = null;
            int i = 0;

            try
            {
                // Write high scores to file
                for (i = 0; i <= 4; i++)
                {
                    Str = Top5PlayerName[i];
                    Str += Environment.NewLine;

                    buffer = new byte[Str.Length];
                    encoder.GetBytes(Str, 0, Str.Length, buffer, 0);
                    objFileStream.Write(buffer, 0, buffer.Length);

                    Str = Top5PlayerScore[i];
                    Str += Environment.NewLine;

                    buffer = new byte[Str.Length];
                    encoder.GetBytes(Str, 0, Str.Length, buffer, 0);
                    objFileStream.Write(buffer, 0, buffer.Length);

                }

                // Close the stream  
                objFileStream.Close();

            }
            catch
            {

            }
        }

        private void flashText(string textToShow, int x, int y, int fSize)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Flash specified text  
            //------------------------------------------------------------------------------------------------------------------

            if (tScales.getTAcaleC() == 0)
                static_flashText_flash += 1;
            static_flashText_flash = static_flashText_flash % 2;

            if (static_flashText_flash == 1)
            {
                DrawText(graphicsBuffer, textToShow, x, y, fSize, FontStyle.Bold, Brushes.Black);
                DrawText(graphicsBuffer, textToShow, x + 2, y + 2, fSize, FontStyle.Bold, Brushes.Red);
            }

        }
        static int static_flashText_flash = 0;

        private void doTitle()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Title screen
            //
            // 1. Render game title
            // 2. Flash F2 to play ...
            // 3. Render animated score table
            //------------------------------------------------------------------------------------------------------------------

            int i = 0;

            // Render title
            graphicsBuffer.DrawImage(this.gameTitle, new Rectangle(145, 85, this.gameTitle.Width, this.gameTitle.Height), 0, 0, 
                this.gameTitle.Width, this.gameTitle.Height, GraphicsUnit.Pixel, ImageAtt);

            //                                        :: F2 to start a new game ::

            // Render star
           // graphicsBuffer.DrawImage(star, new Rectangle(175, 430, 550, 125), 0, 0, star.Width, star.Height, GraphicsUnit.Pixel, ImageAtt);

            // Render text ...
            if (tScales.getTAcaleC() > 10)
            {
                DrawText(graphicsBuffer, "Press F2 To Play with Sound", 307, 325, 16, FontStyle.Bold, Brushes.Black);
                DrawText(graphicsBuffer, "Press F2 To Play with Sound", 309, 327, 16, FontStyle.Bold, Brushes.Yellow);
                DrawText(graphicsBuffer, "Press F3 To Play without Sound", 297, 355, 16, FontStyle.Bold, Brushes.Black);
                DrawText(graphicsBuffer, "Press F3 To Play without Sound", 297, 357, 16, FontStyle.Bold, Brushes.Yellow);


            }

            //                           :: Render animated high score table (top 5 players) ::

            // Table heading: top 5 players
            for (i = 0; i <= 2; i++)
            {
                DrawText(graphicsBuffer, "TOP 5 PLAYERS", 260 + i, 400 + i, 23, FontStyle.Bold, Brushes.DarkRed);
            }
            DrawText(graphicsBuffer, "TOP 5 PLAYERS", 260 + i, 400 + i, 23, FontStyle.Bold, Brushes.Yellow);

            // Score table: names & scores with drop shadow and white face
            for (i = 0; i <= 4; i++)
            {
                // Drop shadow
                DrawText(graphicsBuffer, Top5PlayerName[i], 263, 448 + (i * 30), 16, FontStyle.Regular, Brushes.Black);
                DrawText(graphicsBuffer, Top5PlayerScore[i], 443, 450 + (i * 30), 16, FontStyle.Regular, Brushes.Black);

                // White type face
                DrawText(graphicsBuffer, Top5PlayerName[i], 265, 450 + (i * 30), 16, FontStyle.Regular, Brushes.White);
                DrawText(graphicsBuffer, Top5PlayerScore[i], 445, 450 + (i * 30), 16, FontStyle.Regular, Brushes.White);
            }


            if (false)
            {
                // Every 1000mS select a new entry to alternate between white and red
                if (tScales.getTAcaleD() == 0)
                {
                    static_doTitle_Animate[0] += 1;
                    static_doTitle_Animate[0] = static_doTitle_Animate[0] % 5;
                }

                // Flash rate of ~5Hz ...
                if (tScales.getTAcaleA() == 0)
                {
                    static_doTitle_Animate[1] += 1;
                    static_doTitle_Animate[1] = static_doTitle_Animate[1] % 2;
                }

            }
            // Animate flash each player's name and their score sequentially
            if (static_doTitle_Animate[1] == 1)
            {
                // Drop shadow
                DrawText(graphicsBuffer, Top5PlayerName[i], 163, 448 + (i * 30), 16, FontStyle.Regular, Brushes.Black);
                DrawText(graphicsBuffer, Top5PlayerScore[i], 343, 450 + (i * 30), 16, FontStyle.Regular, Brushes.Black);

                // White type face
                DrawText(graphicsBuffer, Top5PlayerName[static_doTitle_Animate[0]], 165, 450 + (static_doTitle_Animate[0] * 30), 16, FontStyle.Regular, Brushes.Red);
                DrawText(graphicsBuffer, Top5PlayerScore[static_doTitle_Animate[0]], 345, 450 + (static_doTitle_Animate[0] * 30), 16, FontStyle.Regular, Brushes.Red);
            }

        }
        static int[] static_doTitle_Animate = new int[3];


        private void DrawStar(int X, int Y, string Message)
        {
            // Render star graphic
            graphicsBuffer.DrawImage(star, new Rectangle(X, Y, star.Width, star.Height - 50), 0, 0, star.Width, star.Height, GraphicsUnit.Pixel, ImageAtt);
            // Text ...
            DrawText(graphicsBuffer, Message,X+276-(Message.Length*10), Y+60, 22, FontStyle.Bold, Brushes.Black);
            DrawText(graphicsBuffer, Message, X+276-(Message.Length*10), Y+60, 22, FontStyle.Bold, Brushes.Red);
         
        }

        private void doEndOfLevel()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Deal with the completion of a level
            //------------------------------------------------------------------------------------------------------------------

            // Inc counter ...
            genWorkingCounter += 1;

            // Structured code controlled by counter status
            if (genWorkingCounter < 100)
            {
                DrawStar(227,365,"Level Complete!");


                //Abduction complete
                clsShared.global_gravity_warp_on = false;
                clsShared._clsPlayer.setWarpY(0);
                clsShared.Global_Abducted = false;
                clsShared.AbductionPhase = 0;
           

            }
            else if (genWorkingCounter == 100)
            {
                // Play snd effect
                if (!sndEngineError)
                {
                    _clsCommon.sndEffects[5].playSND(false);
                }
            }
            else if (genWorkingCounter < 300)
            {
                player.setDoWarp(true);
            }
            else
            {
                level += 1;
                doingSOLevel = true;
                genWorkingCounter = 0;
                player.setDoWarp(false);
                playerPivotPos = 61;
            }
        }

        private void doStartOfLevel()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Deal with the beginning of a new level 
            //------------------------------------------------------------------------------------------------------------------

            // Inc counter ...
            genWorkingCounter += 1;

            // Render star graphic
    //        graphicsBuffer.DrawImage(star, new Rectangle(27, 265, (int)(star.Width*.33), star.Height - 50), 0, 0, star.Width, star.Height, GraphicsUnit.Pixel, ImageAtt);

            // Structured code controlled by counter status
            if (genWorkingCounter == 1)
            {
              initNewInvaders(false); 
            }
            else if (genWorkingCounter < 100)
            {
                //DrawStar(427, 365, "" + Convert.ToString(level));
    
                flashText("GET READY!", 417, 365, 25);          
            }
            else if (genWorkingCounter < 200)
            {
                DrawStar(227, 365, "Level " + Convert.ToString(level));

               //DrawText(graphicsBuffer, "Level " + Convert.ToString(level), 209, 318, 32, FontStyle.Bold, Brushes.Black);
               //DrawText(graphicsBuffer, "Level " + Convert.ToString(level), 211, 320, 32, FontStyle.Bold, Brushes.Red);
            }
            else
            {
               doingSOLevel = false;
               genWorkingCounter = 0;
            }
        }

        private void doStartOfGame()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Deal with the start of a fresh new game 
            //------------------------------------------------------------------------------------------------------------------

            // Inc counter ...
            genWorkingCounter += 1;

            // Render star graphic
            //graphicsBuffer.DrawImage(star, new Rectangle(27, 265, star.Width, star.Height - 50), 0, 0, star.Width, star.Height, GraphicsUnit.Pixel, ImageAtt);
           // DrawStar(27, 265, "");

            // Structured code controlled by counter status
            if (genWorkingCounter == 1)
            {
               initNewInvaders(true);
            }
            else if (genWorkingCounter < 250)
            {
                if (false)
                {
                    graphicsBuffer.DrawImage(keyInstructions, new Rectangle(180, 300, keyInstructions.Width, keyInstructions.Height), 0, 0, keyInstructions.Width, keyInstructions.Height, GraphicsUnit.Pixel, ImageAtt);
                    DrawText(graphicsBuffer, "SHOOT", 180, 335, 12, FontStyle.Bold, Brushes.White);
                    DrawText(graphicsBuffer, "LEFT", 272, 335, 12, FontStyle.Bold, Brushes.White);
                    DrawText(graphicsBuffer, "RIGHT", 360, 335, 12, FontStyle.Bold, Brushes.White);
                    DrawText(graphicsBuffer, "Game Controls", 225, 355, 14, FontStyle.Bold, Brushes.Red);
                }
            }
            else
            {
               doingSOGame = false;
               doingSOLevel = true;
               genWorkingCounter = 0;
            }
        }

        private void InitSound()
        {
            bkMusic[0] = new clsSound(System.IO.Directory.GetCurrentDirectory() + "\\bgm.wav", SoundEnabled);

            _clsCommon.sndEffects[0] = new clsSound(System.IO.Directory.GetCurrentDirectory() + "\\dive.wav", SoundEnabled);
            _clsCommon.sndEffects[1] = new clsSound(System.IO.Directory.GetCurrentDirectory() + "\\shoot.wav", SoundEnabled);
            _clsCommon.sndEffects[2] = new clsSound(System.IO.Directory.GetCurrentDirectory() + "\\enemyShot.wav", SoundEnabled);
            _clsCommon.sndEffects[3] = new clsSound(System.IO.Directory.GetCurrentDirectory() + "\\explosion.wav", SoundEnabled); 
            _clsCommon.sndEffects[4] = new clsSound(System.IO.Directory.GetCurrentDirectory() + "\\pickup.wav", SoundEnabled);
            _clsCommon.sndEffects[5] = new clsSound(System.IO.Directory.GetCurrentDirectory() + "\\warp.wav", SoundEnabled);

            _clsCommon.sndEffects[6] = new clsSound(System.IO.Directory.GetCurrentDirectory() + "\\gravity.wav", SoundEnabled);

            _clsCommon.sndEffects[7] = new clsSound(System.IO.Directory.GetCurrentDirectory() + "\\abducted.wav", SoundEnabled); 

            bkMusic[0].playSND(true);
            if (SoundEnabled == false)
            {
                bkMusic[0].stopSND();

            }
         
                 
        }
        private void initialize()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Initialize game
            //
            // 1. Check that FMOD is present and functioning
            // 2. Create sprite instances ...
            //------------------------------------------------------------------------------------------------------------------

            int i = 0;
            int j = 0;
            int k = 0;
            int e = 0;
            int c = 0;

            DrawText(graphicsBuffer, ":: Gridcoin Galaza -- ßeta rev: 1.1 -- 4/22/2015 :: ", 20, 50, 12, FontStyle.Regular, Brushes.Red);

            // Precision regulated  delay
            initDelay();
            DrawText(graphicsBuffer, "Precision regulated delay initialized", 20, 100, 12, FontStyle.Regular, Brushes.White);

            // Update screen
            this.Refresh();

            // FMOD sound engine ...
            try
            {
                FSOUND_Init(44100, 32, 0);

                // Good
                DrawText(graphicsBuffer, "FMOD sound engine initialize", 20, 120, 12, FontStyle.Regular, Brushes.White);

            }
            catch
            {
                // Problems
                DrawText(graphicsBuffer, "FMOD sound engine unable to initialize", 20, 120, 12, FontStyle.Regular, Brushes.White);
            //    sndEngineError = true;
            }

            // Update screen
            this.Refresh();

            // Sound effects & music ...
            if (!sndEngineError)
            {
                try
                {
                    InitSound();
                    
                    // Good
                    DrawText(graphicsBuffer, "FMOD sound engine OK", 20, 140, 12, FontStyle.Regular, Brushes.White);


                }
                catch
                {
                    // Problems
                    DrawText(graphicsBuffer, "FMOD sound engine ERROR", 20, 140, 12, FontStyle.Regular, Brushes.White);
                    sndEngineError = true;
                }

                //- Problems ...
            }
            else
            {
                DrawText(graphicsBuffer, "FMOD sound engine ERROR", 20, 140, 12, FontStyle.Regular, Brushes.White);
            }

            // Update
            this.Refresh();

            // Sample of time in mS
            k = timeGetTime();

            // Load resource images and remove backgrounds and thus a sprite is born
            background[0] = GridcoinGalaza.Properties.Resources.background;
            background[1] = GridcoinGalaza.Properties.Resources.backgroundTop1;
            background[2] = GridcoinGalaza.Properties.Resources.backgroundTop1;
            background[3] = GridcoinGalaza.Properties.Resources.backgroundTop3;
            background[4] = GridcoinGalaza.Properties.Resources.backgroundTop4;
            background[5] = GridcoinGalaza.Properties.Resources.backgroundTop5;

            gameTitle = GridcoinGalaza.Properties.Resources.title;
            keyInstructions = GridcoinGalaza.Properties.Resources.keys;
            star = GridcoinGalaza.Properties.Resources.starA;
            gameTitle.MakeTransparent(Color.Black);
            star.MakeTransparent(Color.Black);
            keyInstructions.MakeTransparent(Color.Red);

            // Invader sprites ...
            createInvaderInstances();

            //:: Backgroud scrolling vectors ::

            // -------------------------------------------------------------------------------------
            // 8 Vectors which sequentially scroll until they reach the vector belows stating y-axis
            // -------------------------------------------------------------------------------------
            // 3 hardcoded attributes for each vector
            // 1. Starting y-axis
            // 2. Ending y-axis ...
            // 3. Degree of incrementation p/frame
            // -------------------------------------------------------------------------------------

            // Loop through all vectors and create instances with specified attributes ...
            for (i = 0; i <= 7; i++)
            {
                // Asign attributes based on vector index
                switch (i)
                {
                    case 0:
                        j = 168; // Starting y-axis
                        e = 188; // Ending y-axis ...
                        c = 2;   // Incrementation weight
                        break;

                    case 1:
                        j = 188;
                        e = 228;
                        c = 4;
                        break;

                    case 2:
                        j = 228;
                        e = 288;
                        c = 6;
                        break;

                    case 3:
                        j = 288;
                        e = 368;
                        c = (i + 1) * 2;
                        break;

                    case 4:
                        j = 368;
                        e = 468;
                        c = (i + 1) * 2;
                        break;

                    case 5:
                        j = 468;
                        e = 588;
                        c = (i + 1) * 2;
                        break;

                    case 6:
                        j = 588;
                        e = 728;
                        c = (i + 1) * 2;
                        break;

                    case 7:
                        j = 728;
                        e = 888;
                        c = (i + 1) * 2;
                        break;
                }

                // Create class instance
                vectorScroll[i] = new clsVectorScroll(0, j, c, e);
            }

            // Background stars
            fetchNewStars();

            // Player's ship 
            player = new clsPlayer(0, 0);
            clsShared._clsPlayer = player;
            
            player.setFirePower(1);

            // Paths ...
            paths = new clsPaths();
            j = timeGetTime();
            DrawText(graphicsBuffer, "Sprites created in: " + Convert.ToString(j - k) + "mS", 20, 160, 12, FontStyle.Regular, Brushes.White);
            DrawText(graphicsBuffer, "Done ...", 20, 180, 12, FontStyle.Regular, Brushes.White);
            this.Refresh();

            processStartmS = timeGetTime();
            regulatedDelay(100);
        }

        private void WipeGraphicsBuffer()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Completely erases primary graphics buffer
            //------------------------------------------------------------------------------------------------------------------

            graphicsBuffer.FillRectangle(new SolidBrush(Color.Black), 0, 0,ScreenWidth, ScreenHeight);

        }

        private void form1_Paint(object sender, System.Windows.Forms.PaintEventArgs e)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Render graphics buffer to the form
            //------------------------------------------------------------------------------------------------------------------

            e.Graphics.DrawImage(scrnBufferBmp, 0, 0);
            e.Graphics.ResetTransform();
        }

        private int getRandomNumber(int low, int high)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Returns a random number, between the optional Low and High parameters
            //------------------------------------------------------------------------------------------------------------------

            return randNum.Next(low, high);
        }

        private void frmGame_FormClosing(object sender, System.Windows.Forms.FormClosingEventArgs e)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Pre termination of application event
            //------------------------------------------------------------------------------------------------------------------

            if (AppRunning)
            {
                AppRunning = false;
                e.Cancel = true;
            }
        }

        private void frmGame_FormClosed(object sender, System.Windows.Forms.FormClosedEventArgs e)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Terminate application -- clean up: dispose of resources
            //------------------------------------------------------------------------------------------------------------------

            // Clean up kill instances
            playerbullet = null;
            vectorScroll = null;
            invaders = null;
            particle = null;
            stars = null;
            player = null;
            paths = null;
            _clsCommon.sndEffects = null;
            bkMusic = null;
            pickup = null;

            // Dump screen buffer ...
            scrnBufferBmp.Dispose();
            graphicsBuffer.Dispose();

            // Save high scores ...
            SaveHighScores();

            // free FMOD sound engine from its duty
            if (!sndEngineError)
            {
               
               // FSOUND_Close();
            }

            // Dump this instance and terminate 
            this.Dispose();
            System.Environment.Exit(0);

        }
    }
}
