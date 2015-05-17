//   -
// Purpose:      Particles
// Rev:          1.0
// Last updated: 22/03/10

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;

namespace GridcoinGalaza
{
    public class clsParticle : clsCommon
    {

        // Properties for this class 
        private int frame;
        private bool active;

        // Obj refs and instances
        private System.Random objRandom = new System.Random(Convert.ToInt32(System.DateTime.Now.Ticks % System.Int32.MaxValue));
 
        public clsParticle(int x, int y, int width, int height): base(x, y, width, height)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Class constructor  
            //------------------------------------------------------------------------------------------------------------------

            this.active = true;
        }

        public void setFrameNum(int frame)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set frame num) 
            //------------------------------------------------------------------------------------------------------------------

            this.frame = frame;
        }

        public int getFrameNum()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (fetch frame num)    
            //------------------------------------------------------------------------------------------------------------------

            return this.frame;
        }

        public bool isActive()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (is this particle active?)  
            //------------------------------------------------------------------------------------------------------------------

            return this.active;
        }

        public void draw(Graphics Destination)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method to render a particle explosion  
            //------------------------------------------------------------------------------------------------------------------

            int j = 0;
            double x = 0;
            double y = 0;
            double a = 0;

            // Instance of a drawing colour to use
            System.Drawing.Color starcol = default(System.Drawing.Color);

            // Gradually increase the radius of the particles from the center

            for (a = 0; a <= (2 * Math.PI) - (2.0 * Math.PI / this.frame); a += (2.0 * Math.PI) / this.frame)
            {
                // calculate the x position of the next number
                x = (this.frame / 2) * Math.Cos(a - (Math.PI / 3)) + this.frame / 2;

                // calculate the y position of the next number
                y = (this.frame / 2) * Math.Sin(a - (Math.PI / 3)) + this.frame / 2;

                // Apply x & y axis
                x += Convert.ToInt32(base.getX() + ((base.getW() / 2) - 5));
                y += Convert.ToInt32(base.getY() + ((base.getH() / 2) - 5));

                // Fetch random colour to use
                j = getRandomNumber(0, 2);

                // Apply it
                switch (j)
                {
                    case 0:
                        starcol = Color.Orange;
                        break;
                    case 1:
                        starcol = Color.Yellow;
                        break;
                    case 2:
                        starcol = Color.Red;
                        break;
                }

                // Creat brush instance
                SolidBrush brush = new SolidBrush(starcol);

                // Draw pixels
                Destination.FillRectangle(brush, Convert.ToSingle(x), Convert.ToSingle(y), 2, 2);

                // Clean up
                brush.Dispose();
            }

            // Next frame next game ..
            this.frame += 1;

            // Particle explsoion complete
            if (this.frame == 25)
            {
                this.active = false;
            }
        }

        private int getRandomNumber(int Low, int High)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Returns a random number, between the optional Low and High parameters
            //------------------------------------------------------------------------------------------------------------------

            return objRandom.Next(Low, High + 1);
        }
    }
}
