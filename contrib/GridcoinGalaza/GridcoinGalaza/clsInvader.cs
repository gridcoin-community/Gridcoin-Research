
// Purpose:      Enemy invaders
// Rev:          1.0
// Last updated: 22/03/10

using System;
using System.Drawing;
using System.Drawing.Imaging;

namespace GridcoinGalaza
{
    public class clsInvader : clsCommon
    {

        private int gravitycount = 0;
        private bool gravity_warp_on = false;
        public bool ship_abducted = false;
        public int ScreenHeight = 900;
        private double abduction_Y = 0;
        // Obj refs and instances
        private System.Drawing.Bitmap[,] invader = new System.Drawing.Bitmap[6, 2];
        private System.Drawing.Bitmap[] background = new System.Drawing.Bitmap[7];
        private ImageAttributes ImageAtt = new ImageAttributes();
        private clsTScales tScales = new clsTScales();
        private clsCommon soundSurrogate = null;
        // Properties for this class 
        private int invaderType;
        private int dockingCell;
        private int diveFrame;
        private int playerX;
        private int diveX;
        private int diveType;
        private bool lockedOn;
        private bool lockOnX1;
        private bool lockOnX2;
        private bool shoot;
        
        private bool docked;
        private bool docking;
        private bool reDocking;
        private bool rotate;
        private bool dive;
        private bool active;
        private bool hasCell;
        private bool twirl;
        private double angle;

        // Class refs ...
        private clsInvaderBullet bullet;
        private clsInvaderBullet bullet2;

        public clsInvader(int x, int y, int invaderType, bool active, clsCommon SoundSurrogate): base(x, y, 0, 0)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Class constructor  
            //------------------------------------------------------------------------------------------------------------------
            soundSurrogate = SoundSurrogate;

            // Assign properties with values
            this.invaderType = invaderType;
            this.active = active;

            //:: Load resource image(s) & remove background 
            invader[0, 0] = GridcoinGalaza.Properties.Resources.enemyD1;
            invader[0, 1] = GridcoinGalaza.Properties.Resources.enemyD2;
            invader[1, 0] = GridcoinGalaza.Properties.Resources.enemyA1;
            invader[1, 1] = GridcoinGalaza.Properties.Resources.enemyA2;
            invader[2, 0] = GridcoinGalaza.Properties.Resources.enemyE1;
            invader[2, 1] = GridcoinGalaza.Properties.Resources.enemyE2;
            invader[3, 0] = GridcoinGalaza.Properties.Resources.enemyB1;
            invader[3, 1] = GridcoinGalaza.Properties.Resources.enemyB2;
            invader[4, 0] = GridcoinGalaza.Properties.Resources.enemyC1;
            invader[4, 1] = GridcoinGalaza.Properties.Resources.enemyC2;
            invader[5, 0] = GridcoinGalaza.Properties.Resources.enemyF1;
            invader[5, 1] = GridcoinGalaza.Properties.Resources.enemyF2;

            // Remove backgrounds ...
            int i = 0;
            int j = 0;

            for (j = 0; j <= 1; j++)
            {
                for (i = 0; i <= 5; i++)
                {
                    invader[i, j].MakeTransparent(Color.White);
                }
            }
        }

        public void doDive()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method to dive straight down towards player  
            //-----------------------------------------------------------------------------------------------------------------

            double x = 0;
            double y = 0;
            double deg = 360 / 12;
            double radius = 25;

            // Inc time scalers
            tScales.setTScaleB(tScales.getTAcaleB() + 1);
            tScales.setTScaleB(tScales.getTAcaleB() % 10);

            //:: Rotate from dock at start of dive ::
            if (this.twirl == false)
            {
                // Type val of 0 denotes a rotate before dive, otherwise a straight down dive
                if (this.getDiveType() == 0)
                {
                    // 10 Frames of it
                    if (this.getDiveFrame() < 10)
                    {
                        if (tScales.getTAcaleB() == 0)
                        {
                            // Fetch x & y
                            x = GetCos(this.getDiveFrame() * deg + 90) * radius;
                            y = GetSin(this.getDiveFrame() * deg + 90) * radius;

                            // Apply coords
                            base.setX(base.getX() + -Convert.ToInt32(x));
                            base.setY(base.getY() + -Convert.ToInt32(y));

                            // Inc dive frame counter
                            this.setDiveFrame(this.getDiveFrame() + 1);
                        }
                    }
                    else
                    {
                        // Now do twirl towards player
                        this.twirl = true;
                    }

                }
                else
                {
                    // Dive type is straight down
                    this.twirl = true;
                }

                //- Lock on to player's x-axis -
            }
            else
            {

                if (!lockedOn)
                {
                    // Inc / dec x-axis until we find player's ship
                    if (base.getX() < playerX)
                    {
                        x += 3;
                        lockOnX1 = true;
                    }
                    else
                    {
                        x -= 3;
                        lockOnX2 = true;
                    }

                    // Locked on, set flag ...
                    if (lockOnX1 & lockOnX2)
                    {
                        lockedOn = true;
                    }
                }

                //:: Dive towards player ::

                // Twirl back and forth
                diveX += 1;
                diveX = diveX % 64;
                if (diveX < 32)
                    x -= 2;
                if (diveX > 32)
                    x += 2;

                // Apply x & y locations ...
                base.setX(base.getX() + Convert.ToInt32(x));
                //4-19-2015
                
                if (diveX % 50 == 1)
                {
                    if (this.invaderType == 3 && base.getY() > 500 && !gravity_warp_on && !clsShared.global_gravity_warp_on
                        && !clsShared._clsPlayer.DoubleWideShip && clsShared.AbductionPhase == 0 && !clsShared.Global_Abducted)
                   {

                       if (!clsShared._clsPlayer.isDead() && !clsShared._clsPlayer.isDoingWarp())
                       {

                           //3 == Green Invader
                           //2 == Yellow Invader
                           gravity_warp_on = true;
                           clsShared.global_gravity_warp_on = true;
                           soundSurrogate.sndEffects[6].playSND(false);
                       }
                    }
                }

                if (!gravity_warp_on)               base.setY(base.getY() + 3);
                



                // Invader off the screen? -- reset and prepare to redock
                if (base.getY() > ScreenHeight)
                {
                    ReDockAlien();
                }
            }
        }

        public void ReDockAlien()
        {
            this.diveFrame = 0;
            this.diveX = 0;
            this.dive = false;
            this.docking = true;
            this.reDocking = true;
            this.lockedOn = false;
            this.lockOnX1 = false;
            this.lockOnX2 = false;
            this.twirl = false;
            base.setX(300);
            base.setY(0);
             
        }

        public void CheckForGravity(Graphics Destination)
        {
            //Halford 4-18-2015 If Invader Type is the B1 or C1, and dive is halfway down, and Gravity Field is applicable, start gravity field:
            if (gravity_warp_on)
            {
                double WIDTH = 100;
                int ship_y = (int)(clsShared._clsPlayer.getY() - abduction_Y);
                for (int yy = base.getY(); yy < ship_y; yy += 3)
                {
                    WIDTH = WIDTH - .5;
                    if (WIDTH < 5) WIDTH = 5;
                    DrawGravity(Destination, base.getX() - (int)(WIDTH / 2), yy + 20, (int)WIDTH);
                }
                //Check to see if player has been abducted
                if (clsShared._clsPlayer.getX() > base.getX() - 20 && clsShared._clsPlayer.getX() < base.getX() + 20 && clsShared.AbductionPhase == 0)
                {
                    clsShared.AbductionPhase = 1;
                    clsShared.iAbductionCounter = 0;
                    //Abduct the ship
                    soundSurrogate.sndEffects[6].playSND(false);
                    soundSurrogate.sndEffects[7].playSND(false);  //Abduction loop
                    abduction_Y=0;

                  //  clsShared._clsPlayer.setDoWarp(true);
                }
                else if (clsShared.AbductionPhase == 0)
                    {
                        //Ship is not in the field, but gravity field is on
                        soundSurrogate.sndEffects[6].playSND(false);
                 
                    }


                if (clsShared.AbductionPhase == 1)
                {

                    soundSurrogate.sndEffects[7].playSND(false);  //Abduction loop
                    
                    //Abduct the ship towards the alien
                    abduction_Y += 4;
                    this.setY(this.getY() - 3);
                    if (clsShared._clsPlayer.getY() - abduction_Y < 75) abduction_Y = abduction_Y - 5;
                    clsShared._clsPlayer.setWarpY((int)abduction_Y);
                    if (this.getY() < 50) this.setY(50);
                    if (this.getY() < 75  && (clsShared._clsPlayer.getY()-abduction_Y) < 100)
                    {
                        //Abduction complete
                        clsShared.lives -= 1;
                        gravity_warp_on = false;
                        clsShared.global_gravity_warp_on = false;
                        clsShared._clsPlayer.setWarpY(0);
                        ship_abducted = true;
                        clsShared.Global_Abducted = true;
                        soundSurrogate.sndEffects[6].playSND(false); //WARP SOUND
                        clsShared.AbductionPhase = 0;
                        ReDockAlien();
                        //If we abducted last ship, game is over:
                        if (clsShared.lives < -1)
                        {
                            clsShared.gameOver = true;
                        }
                   
                    }


                }


            }
                
        }

        public void DrawGravity(Graphics Destination, int X, int Y, int WIDTH)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method to render pixels to graphics buffer  
            //------------------------------------------------------------------------------------------------------------------

            // Creat brush instance
            SolidBrush brush = new SolidBrush(Color.White);

            // Draw sprite
            Destination.FillRectangle(brush, X, Y, WIDTH, 1);

            // Clean up
            brush.Dispose();
        }
   


        private double GetSin(double degAngle)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method (return sin)   
            //------------------------------------------------------------------------------------------------------------------

            return Math.Sin(Math.PI * degAngle / 180);
        }

        private double GetCos(double degAngle)
        {
            //------------------------------------------------------------------------------------------------------------------
            //  Purpose: Method (return cosin)   
            //------------------------------------------------------------------------------------------------------------------

            return Math.Cos(Math.PI * degAngle / 180);
        }

        public int getDiveType()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch dive type: targeted or just straight down)   
            //------------------------------------------------------------------------------------------------------------------

            return this.diveType;
        }

        public bool hasDockingCell()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (does this invader have a docking cell allocated?)   
            //------------------------------------------------------------------------------------------------------------------

            return this.hasCell;
        }

        public bool isActive()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (is this invader active?)  
            //------------------------------------------------------------------------------------------------------------------

            return this.active;
        }

        public void setActive(bool active)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set invader active)   
            //-----------------------------------------------------------------------------------------------------------------

            this.active = active;
        }

        public bool isRedocking()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (is this invader docking?)   
            //------------------------------------------------------------------------------------------------------------------

            return this.reDocking;
        }

        public void setRedocking(bool reDocking)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set invader to do re-docking after a dive)  
            //-----------------------------------------------------------------------------------------------------------------

            this.reDocking = reDocking;
        }

        public int getInvaderType()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch invader type)  
            //------------------------------------------------------------------------------------------------------------------

            return this.invaderType;
        }

        public void setInvaderType(int invaderType)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set invader type)   
            //-----------------------------------------------------------------------------------------------------------------

            this.invaderType = invaderType;
            base.setH(this.invader[invaderType, 0].Height);
            base.setW(this.invader[invaderType, 0].Width);
        }

        public bool isDiving()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (is this invader currently diving?)   
            //------------------------------------------------------------------------------------------------------------------

            return this.dive;
        }

        public void setDoDiving(bool dive, int playerX, int diveType)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set invader to init diving)  
            //-----------------------------------------------------------------------------------------------------------------
            if (ship_abducted) return;  //Dont allow alien holding an abducted ship to Dive


            this.dive = dive;
            this.playerX = playerX;
            this.diveType = diveType;
        }

        public int getDiveFrame()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch current dive frame)  
            //------------------------------------------------------------------------------------------------------------------

            return this.diveFrame;
        }

        public void setDiveFrame(int diveFrame)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Purpose: Mutator (set invader dive frame)   
            //-----------------------------------------------------------------------------------------------------------------

            this.diveFrame = diveFrame;
        }

        public bool isShooting()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (is this invader shooting?)  
            //------------------------------------------------------------------------------------------------------------------

            return this.shoot;
        }

        public void setDoShooting(bool shoot)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method (set invader to init shooting)   
            //-----------------------------------------------------------------------------------------------------------------

            if (shoot)
            {
                this.bullet = new clsInvaderBullet(base.getX() + 10, base.getY());
                this.shoot = shoot;

                this.bullet2 = new clsInvaderBullet(base.getX() + 7, base.getY());
                //this.shoot2 = shoot;

            }
            else
            {
                this.bullet = null;
                this.bullet2 = null;
                this.shoot = false;
            }
        }

        public bool isDocking()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (is this invader docking?)  
            //------------------------------------------------------------------------------------------------------------------

            return this.docking;
        }

        public void setDoDocking(bool docking)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set invader to dock)  
            //------------------------------------------------------------------------------------------------------------------

            this.docking = docking;
        }

        public int getDockingCell()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch dock cell num)  
            //------------------------------------------------------------------------------------------------------------------

            return this.dockingCell;
        }

        public void setDockingCell(int dockingCell)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (assign an invader with a specific docking cell num)  
            //------------------------------------------------------------------------------------------------------------------

            this.dockingCell = dockingCell;
            this.hasCell = true;
        }

        public bool isDocked()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (is this invader docked?)   
            //------------------------------------------------------------------------------------------------------------------

            return this.docked;
        }

        public void setDocked(bool docked)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set invader docked)   
            //------------------------------------------------------------------------------------------------------------------

            this.docked = docked;
        }

        public bool isRotating()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (is this invader rotating?)  
            //------------------------------------------------------------------------------------------------------------------

            return this.rotate;
        }

        public void setDoRotate(bool rotate)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set invader to rotate)   
            //------------------------------------------------------------------------------------------------------------------

            this.rotate = rotate;
        }

        public double getAngle()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch current rotation angle)   
            //------------------------------------------------------------------------------------------------------------------

            return this.angle;

        }

        public void setAngle(double angle)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set invader angle)   
            //------------------------------------------------------------------------------------------------------------------

            this.angle = angle;
        }

        public int getBulletRectX()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch invader bullet collision rect x axis) 
            //------------------------------------------------------------------------------------------------------------------

            return this.bullet.getRectX();
        }

        public int getBulletRectY()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch invader bullet collision rect y axis) 
            //------------------------------------------------------------------------------------------------------------------

            return this.bullet.getRectY();
        }

        public int getBulletRectW()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch invader bullet collision rect width) 
            //------------------------------------------------------------------------------------------------------------------

            return this.bullet.getRectW();
        }

        public int getBulletRectH()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch invader bullet collision rect height) 
            //------------------------------------------------------------------------------------------------------------------

            return this.bullet.getRectH();
        }

        public void doShoot(Graphics Destination)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method to control the invaders bullets 
            //------------------------------------------------------------------------------------------------------------------


            if (this.bullet.getY() < ScreenHeight)
            {
                // Shift bullets down the screen
                if (this.bullet != null)
                {
                    this.bullet.moveBullets(Destination,0);
                    this.bullet2.moveBullets(Destination,2);
                }
            }
            else //- bullet has moved off the screen then kill off instance
            {
                this.bullet = null;
                this.shoot = false;
                this.bullet2 = null;
            }

        }

        public void Draw(Graphics Destination, double angle, int ani, bool rotate)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: method to draw invader sprites with or with rotation and sync collision rect 
            //
            // Rotation method obtained from:
            // http://jelle.druyts.net/2004/05/26/RotatingAnImageAroundItsCenterInNET.aspx
            //------------------------------------------------------------------------------------------------------------------
            CheckForGravity(Destination);

            // Apply width and height for this invader
            base.setW(this.invader[this.invaderType, ani].Width);
            base.setH(this.invader[this.invaderType, ani].Height);


            if (rotate)
            {
                // Rotate the coordinates of the rectangle's corners
                Point upperLeft = RotatePoint(new Point(-invader[this.invaderType, ani].Width / 2, invader[this.invaderType, ani].Height / 2), angle);
                Point upperRight = RotatePoint(new Point(invader[this.invaderType, ani].Width / 2, invader[this.invaderType, ani].Height / 2), angle);
                Point lowerLeft = RotatePoint(new Point(-invader[this.invaderType, ani].Width / 2, -invader[this.invaderType, ani].Height / 2), angle);

                // Create the points array by offsetting the coordinates with the center
                Point[] points = {upperLeft, upperRight, lowerLeft};
        

                // Draw the rotated image
                Destination.DrawImage(invader[this.invaderType, ani], points);

                if (ship_abducted)
                {
                    Destination.DrawImage(clsShared._clsPlayer.player[0],
                        new Rectangle(base.getX() + 30, base.getY() - 30,
                        clsShared._clsPlayer.player[0].Width,
                        clsShared._clsPlayer.player[0].Height),
                        0, 0, clsShared._clsPlayer.player[0].Width, clsShared._clsPlayer.player[0].Height, GraphicsUnit.Pixel,
                        clsShared._clsPlayer.ImagingAtt);

                }
      
                
            }
            else // Render invader with no rotation applied
            {

                Destination.DrawImage(invader[this.invaderType, ani], new Rectangle(base.getX(), base.getY(), invader[this.invaderType, ani].Width, invader[this.invaderType, ani].Height), 0, 0, invader[this.invaderType, ani].Width, invader[this.invaderType, ani].Height, GraphicsUnit.Pixel, ImageAtt);

                if (ship_abducted)
                {
                    Destination.DrawImage(clsShared._clsPlayer.player[0], 
                        new Rectangle(base.getX()+30, base.getY()-30,
                        clsShared._clsPlayer.player[0].Width, 
                        clsShared._clsPlayer.player[0].Height),
                        0, 0, clsShared._clsPlayer.player[0].Width, clsShared._clsPlayer.player[0].Height, GraphicsUnit.Pixel,
                        clsShared._clsPlayer.ImagingAtt);

                }
            }

            // Sync collision rect ...
            base.setRectW(base.getW());
            base.setRectH(base.getH());
            base.setRectX(base.getX());
            base.setRectY(base.getY());
        }

        public Point RotatePoint(Point p, double angle)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method rotates a point around the origin by the specified angle (in radians) 
            // 1 radians = 57.2957795 degrees
            //
            // Rotation method obtained from:
            // http://jelle.druyts.net/2004/05/26/RotatingAnImageAroundItsCenterInNET.aspx
            //------------------------------------------------------------------------------------------------------------------

            // Fetch coords
            int x = Convert.ToInt32(p.X * Math.Cos(angle) + p.Y * Math.Sin(angle));
            int y = Convert.ToInt32(-p.X * Math.Sin(angle) + p.Y * Math.Cos(angle));

            // return them ...
            return new Point(x + base.getX() + (base.getW() / 2), y + base.getY() + (base.getH() / 2));
        }

        public void resetAll()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method to null all properties for a new level ...
            // Creating entirely new instances of this class for every new level is very ineffecient, taking around 500mS on a P4
            //------------------------------------------------------------------------------------------------------------------

            // Super class
            base.setX(-50);
            base.setY(0);
            base.setRectX(base.getX());
            base.setRectY(base.getY());

            // This class
            this.diveFrame = 0;
            this.playerX = 0;
            this.diveX = 0;
            this.lockedOn = false;
            this.lockOnX1 = false;
            this.lockOnX2 = false;
            this.shoot = false;
            this.docked = false;
            this.docking = false;
            this.reDocking = false;
            this.rotate = false;
            this.dive = false;
            this.hasCell = false;
            this.twirl = false;
            this.angle = 0;
            this.dockingCell = 0;
        }
    }
}
