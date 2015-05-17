//   - 
// Purpose:      Background stars
// Rev:          1.0
// Last updated: 22/03/10

using System;
using System.Drawing;
using System.Drawing.Imaging;

namespace GridcoinGalaza
{
    public class clsStars : clsCommon
    {

        // Obj refs and instances
        private System.Drawing.Color starCol;

        public clsStars(int x, int y, System.Drawing.Color starCol): base(x, y, 0, 0)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Class constructor  
            //------------------------------------------------------------------------------------------------------------------

            // Set properties
            this.starCol = starCol;
        }

        public void Draw(Graphics Destination)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method to render pixels to graphics buffer  
            //------------------------------------------------------------------------------------------------------------------

            // Creat brush instance
            SolidBrush brush = new SolidBrush(this.starCol);

            // Draw sprite
            Destination.FillRectangle(brush, base.getX(), base.getY(), 2, 2);

            // Clean up
            brush.Dispose();
        }
    }
}
