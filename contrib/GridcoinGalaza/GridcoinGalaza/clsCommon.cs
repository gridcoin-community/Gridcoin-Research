//   
// Purpose:      Super class (common generic attributes used throughout entire game)
// Rev:          1.0
// Last updated: 22/03/10

using System;
using System.Drawing;

namespace GridcoinGalaza
{

    public static class clsShared
    {
        public static bool Global_Abducted = false;
        public static clsPlayer _clsPlayer = null;
        public static int AbductionPhase = 0;
        public static bool global_gravity_warp_on = false;
        public static int lives = 2;
        public static bool gameOver = true;
        public static long iAbductionCounter = 0;

        
    }

    public class clsCommon
    {
        // Properties for this class 
        private int X;
        private int Y;
        private int H;
        private int W;

        private int X2;
        private int Y2;
      
        public clsSound[] sndEffects = new clsSound[8];
      
        // Obj refs and instances
        private Rectangle rect = new Rectangle();

        public clsCommon(int X, int Y, int W, int H)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Class constructor  
            //------------------------------------------------------------------------------------------------------------------

            this.X = X;
            this.Y = Y;
            this.H = H;
            this.W = W;
        }

        public void setY(int Y)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set y axis) 
            //------------------------------------------------------------------------------------------------------------------

            this.Y = Y;
        }


        public void setY2(int Y)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set y axis) 
            //------------------------------------------------------------------------------------------------------------------

            this.Y2 = Y;
        }


        public void setX(int X)
        {
            this.X = X;
        }

        public void setX2(int X)
        {
            this.X2 = X;
        }

        public int getY()
        {
            return this.Y;
        }

        public int getX()
        {
            return this.X;
        }



        public int getY2()
        {
            return this.Y2;
        }

        public int getX2()
        {
            return this.X2;
        }



        public int getW()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch width)  
            //------------------------------------------------------------------------------------------------------------------

            return this.W;

        }

        public int getH()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch height)  
            //------------------------------------------------------------------------------------------------------------------

            return this.H;

        }

        public void setH(int H)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set height)   
            //------------------------------------------------------------------------------------------------------------------

            this.H = H;

        }

        public void setW(int W)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set width)  
            //------------------------------------------------------------------------------------------------------------------

            this.W = W;

        }

        public void setRectX(int X)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set collision rect x)   
            //------------------------------------------------------------------------------------------------------------------

            this.rect.X = X;

        }

        public int getRectX()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch collision rect x axis) 
            //------------------------------------------------------------------------------------------------------------------

            return this.rect.X;

        }

        public void setRectY(int Y)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set collision rect y)  
            //------------------------------------------------------------------------------------------------------------------

            this.rect.Y = Y;

        }

        public int getRectY()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch collision rect y axis) 
            //------------------------------------------------------------------------------------------------------------------

            return this.rect.Y;

        }

        public void setRectW(int W)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set collision rect width) 
            //------------------------------------------------------------------------------------------------------------------

            this.rect.Width = W;

        }

        public int getRectW()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch collision rect width) 
            //------------------------------------------------------------------------------------------------------------------

            return this.rect.Width;

        }

        public void setRectH(int H)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set collision rect height)  
            //------------------------------------------------------------------------------------------------------------------

            this.rect.Height = H;

        }

        public int getRectH()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch collision rect height)  
            //------------------------------------------------------------------------------------------------------------------

            return this.rect.Height;

        }
    }
}
