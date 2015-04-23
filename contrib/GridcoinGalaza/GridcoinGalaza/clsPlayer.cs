//   - 
// Purpose:      Player's ship
// Rev:          1.0
// Last updated: 22/03/10

using System;
using System.Drawing;
using System.Drawing.Imaging;

namespace GridcoinGalaza
{ 
    public class clsPlayer : clsCommon
    {

        // Obj refs and instances
        public System.Drawing.Bitmap[] player = new System.Drawing.Bitmap[2];
        private System.Drawing.Bitmap[] playerSheild = new System.Drawing.Bitmap[2];
        private System.Drawing.Bitmap[] explosion = new System.Drawing.Bitmap[6];
        public ImageAttributes ImagingAtt = new ImageAttributes();
        private Rectangle sheildRect = new Rectangle();
        private clsTScales tScales = new clsTScales();
        private int blowingup = 0;
        public bool DoubleWideShip = false;

        // Properties for this class 
        private int[] pivotX = new int[129];
        private int[] pivotY = new int[129];
        private int firePower;
        private int sheildTime;
        private int x3FireAmmo;
        private int x5FireAmmo;
        private int sheildX;
        private int sheildY;
        private int playerAni;
        private int sheildAni;
        private int warpW;
        private int warpY;
        private int warpH;
        private bool sheild;
        private bool warp;
        private bool dead;

        public clsPlayer(int x, int y): base(x, y, 0, 0)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Class constructor  
            //------------------------------------------------------------------------------------------------------------------

            //:: Load resource image(s) & remove background and thu a sprite is born ::

            // Player images
            player[0] = GridcoinGalaza.Properties.Resources.playerA;
            player[1] = GridcoinGalaza.Properties.Resources.playerB;

            // Explosion images
            explosion[0] = GridcoinGalaza.Properties.Resources.explosion5;
            explosion[1] = GridcoinGalaza.Properties.Resources.explosion4;
            explosion[2] = GridcoinGalaza.Properties.Resources.explosion3;
            explosion[3] = GridcoinGalaza.Properties.Resources.explosion2;
            explosion[4] = GridcoinGalaza.Properties.Resources.explosion1;

            // Sheild pickup ...
            playerSheild[0] = GridcoinGalaza.Properties.Resources.sheildA;
            playerSheild[1] = GridcoinGalaza.Properties.Resources.sheildB;

            // Remove backgrounds
            player[0].MakeTransparent(Color.White);
            player[1].MakeTransparent(Color.White);
            playerSheild[0].MakeTransparent(Color.Red);
            playerSheild[1].MakeTransparent(Color.Red);

            for (int i = 0; i <= 4; i++)
            {
                explosion[i].MakeTransparent(Color.White);
            }

            // Load piviot dat
            this.loadPivotDat();
        }


        public void SetYBackDoor(int Y)
        {
            base.setY(Y);
        }

        public bool isDead()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (is the player dead?)  
            //------------------------------------------------------------------------------------------------------------------

            return this.dead;
        }

        public void setPlayerDead(bool dead)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set player dead or alive)  
            //------------------------------------------------------------------------------------------------------------------

            this.dead = dead;
        }

        public bool isDoingWarp()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (player doing warp?)  
            //------------------------------------------------------------------------------------------------------------------

            return this.warp;
        }

        public void setDoWarp(bool warp)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set player doing end of level warp) 
            //------------------------------------------------------------------------------------------------------------------

            this.warp = warp;
        }

        public void setWarpW(int w)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set warp width) 
            //------------------------------------------------------------------------------------------------------------------

            this.warpW = w;
        }


        public void setWarpY(int Y)
        {
            this.warpY = Y;
        }
        public void setWarpH(int h)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (height) 
            //------------------------------------------------------------------------------------------------------------------

            this.warpH = h;
        }

        public int getWarpW()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch warp width)  
            //------------------------------------------------------------------------------------------------------------------

            return this.warpW;
        }

        public int getWarpH()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (height)  
            //------------------------------------------------------------------------------------------------------------------

            return this.warpH;
        }

        public void setFirePower(int firePower)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set player's level of fire power)  
            //------------------------------------------------------------------------------------------------------------------

            this.firePower = firePower;
        }

        public int getFirePowerLevel()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch player's level of fire power)  
            //------------------------------------------------------------------------------------------------------------------

            return this.firePower;
        }

        public bool hasSheild()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (has the player got the sheild pickup?)  
            //------------------------------------------------------------------------------------------------------------------

            return this.sheild;
        }

        public void setSheild(bool sheild)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set player with / without sheild)  
            //------------------------------------------------------------------------------------------------------------------

            this.sheild = sheild;
        }

        public void setSheildTime(int sheildTime)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set player's time with having a sheild)  
            //------------------------------------------------------------------------------------------------------------------

            this.sheildTime = sheildTime;
        }

        public int getSheildTime()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch the amount of time that the player has had a sheild for)  
            //------------------------------------------------------------------------------------------------------------------

            return this.sheildTime;
        }

        public void setX3FireAmmo(int x3FireAmmo)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set player's num of bullets remaining with this weapon)  
            //------------------------------------------------------------------------------------------------------------------

            this.x3FireAmmo = x3FireAmmo;
        }

        public int getX3FireAmmo()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch ammo remaining with this weapon)  
            //------------------------------------------------------------------------------------------------------------------

            return this.x3FireAmmo;
        }

        public void setX5FireAmmo(int x5FireAmmo)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set player's num of bullets remaining with this weapon)    
            //------------------------------------------------------------------------------------------------------------------

            this.x5FireAmmo = x5FireAmmo;
        }

        public int getX5FireAmmo()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch ammo remaining with this weapon)  
            //------------------------------------------------------------------------------------------------------------------

            return this.x5FireAmmo;
        }

        public void setSheildX(int X)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set sheild location x)  
            //------------------------------------------------------------------------------------------------------------------

            this.sheildX = X;
        }

        public void setSheildY(int Y)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (location y)  
            //------------------------------------------------------------------------------------------------------------------

            this.sheildY = Y;
        }

        public int getSheildX()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch x)  
            //------------------------------------------------------------------------------------------------------------------

            return this.sheildX;
        }

        public int getSheildY()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch y) 
            //------------------------------------------------------------------------------------------------------------------

            return this.sheildY;
        }

        public void setSheildRectX(int X)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set collision rect x)   
            //------------------------------------------------------------------------------------------------------------------

            this.sheildRect.X = X;
        }

        public void setSheildRectY(int Y)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set collision rect y)    
            //------------------------------------------------------------------------------------------------------------------

            this.sheildRect.Y = Y;
        }

        public void setSheildRectH(int H)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set collision rect height)    
            //------------------------------------------------------------------------------------------------------------------

            this.sheildRect.Height = H;
        }

        public void setSheildRectW(int W)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (width)   
            //------------------------------------------------------------------------------------------------------------------

            this.sheildRect.Width = W;
        }

        public int getSheildRectX()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch collision rect x)  
            //------------------------------------------------------------------------------------------------------------------

            return this.sheildRect.X;
        }

        public int getSheildRectY()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (y)   
            //------------------------------------------------------------------------------------------------------------------

            return this.sheildRect.Y;
        }

        public int getSheildRectW()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch collision rect width)   
            //------------------------------------------------------------------------------------------------------------------

            return this.sheildRect.Width;
        }

        public int getSheildRectH()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (height)   
            //------------------------------------------------------------------------------------------------------------------

            return this.sheildRect.Height;
        }

        public int getPivotX(int index)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch player x axis to pivot on) 
            //------------------------------------------------------------------------------------------------------------------

            return this.pivotX[index];
        }

        public int getPivotY(int index)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch player y axis to pivot on)   
            //------------------------------------------------------------------------------------------------------------------

            return this.pivotY[index];
        }

        public void Draw(Graphics Destination, bool doExplosion, int enabled)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: method to draw player's sprite and sync collision rect  
            //------------------------------------------------------------------------------------------------------------------

            if (enabled == 0)
            {
                // Inc time scalers
                tScales.setTScaleB(tScales.getTAcaleB() + 1);
                tScales.setTScaleB(tScales.getTAcaleB() % 10);

                // Animate player / explosions
                if (tScales.getTAcaleB() == 0)
                {
                    playerAni += 1;
                }

                // Either do explsion graphics or draw player's ship
                if (doExplosion == true)
                {
                    // Draw exposion sprites ...
                    playerAni = playerAni % 5;
                    blowingup++;

                    int playeralt = blowingup % 10;
                    if (playeralt < 8)
                    {
                        Destination.DrawImage(explosion[playerAni],
                            new Rectangle(base.getX() + Convert.ToInt32((base.getRectW() / 2) - (explosion[playerAni].Width / 2)),
                                base.getY() + 15,
                                explosion[playerAni].Width,
                                explosion[playerAni].Height), 0, 0,
                                explosion[playerAni].Width, explosion[playerAni].Height, GraphicsUnit.Pixel, ImagingAtt);
                    }
                    else
                    {
                        Destination.DrawImage(explosion[playerAni],
                            new Rectangle(base.getX() + Convert.ToInt32((base.getRectW() / 2) - (explosion[playerAni].Width / 2)),
                                base.getY() + 15,
                                (int)(explosion[playerAni].Width * .5),
                                (int)(explosion[playerAni].Height * .5)), 0, 0,
                                (int)(explosion[playerAni].Width * 1), (int)(explosion[playerAni].Height * 1), GraphicsUnit.Pixel, ImagingAtt);
                    }

                }
                else //- Draw the player's ship -
                {
                    // Animate & render ...
                    playerAni = playerAni % 2;
                    Destination.DrawImage(player[playerAni], new Rectangle(base.getX(), base.getY()-this.warpY, 
                        player[playerAni].Width - this.warpW, player[playerAni].Height - this.warpH), 0, 0, player[playerAni].Width, player[playerAni].Height, GraphicsUnit.Pixel, ImagingAtt);

                    //If the player has a double wide ship:
                    if (DoubleWideShip)
                    {
                        Destination.DrawImage(player[playerAni], new Rectangle(base.getX() + player[playerAni].Width, base.getY() - this.warpY,
                            player[playerAni].Width - this.warpW, player[playerAni].Height - this.warpH), 0, 0, player[playerAni].Width, player[playerAni].Height, GraphicsUnit.Pixel, ImagingAtt);
                    }
                    
                    // Sync collision rect with player's ship
                    base.setRectW(player[1].Width - 5);
                    base.setRectH(player[1].Height - 20);
                    base.setRectX(base.getX() + 2);
                    base.setRectY(base.getY() + 20);

                    // Sheild?
                    if (this.hasSheild())
                    {
                        // Animate player's sheild
                        if (tScales.getTAcaleB() == 0)
                        {
                            sheildAni += 1;
                            sheildAni = sheildAni % 2;
                        }

                        // Apply x & y
                        this.setSheildX(base.getX() - 8);
                        this.setSheildY(base.getY() - 20);

                        // Render sheild to graphics buffer ...
                        Destination.DrawImage(playerSheild[sheildAni], new Rectangle(this.getSheildX(), this.getSheildY(), playerSheild[sheildAni].Width, playerSheild[sheildAni].Height), 0, 0, playerSheild[sheildAni].Width, playerSheild[sheildAni].Height, GraphicsUnit.Pixel, ImagingAtt);

                        // Sync collision rect with player's sheild
                        this.setSheildRectX(this.getSheildX());
                        this.setSheildRectY(this.getSheildY());
                        this.setSheildRectW(this.playerSheild[sheildAni].Width);
                        this.setSheildRectH(this.playerSheild[sheildAni].Height);

                    }
                }
            }
        }       

        private void loadPivotDat()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method to load piviot data   
            //------------------------------------------------------------------------------------------------------------------
            int startX = 980;
            int startY = 725;
            double currentX = startX;

            for (int i = 0; i <= 121; i++)
            {
                pivotY[i] = startY;
                pivotX[i] = (int)currentX;
                currentX -= startX/121;
            }

            if (false)
            {

            }
        }
    }
}
