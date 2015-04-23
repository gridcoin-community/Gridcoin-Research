//   - 
// Purpose:      Vector
// Rev:          1.0
// Last updated: 22/03/10

using System;
using System.Drawing;
using System.Drawing.Imaging;

namespace GridcoinGalaza
{
    public class clsVectorScroll : clsCommon
    {

        // Properties for this class 
        private int incAmount;
        private int startingY;
        private int finishY;

        public clsVectorScroll(int x, int y, int incAmount, int finishY): base(x, y, 0, 0)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Class constructor  
            //------------------------------------------------------------------------------------------------------------------
            this.startingY = y;
            this.finishY = finishY;
            this.incAmount = incAmount;
        }

        public void setIncAmount(int incAmount)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Mutator (set scroll incDelay)  
            //------------------------------------------------------------------------------------------------------------------

            this.incAmount = incAmount;
        }

        public int getIncAmount()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (scroll incDelay)   
            //------------------------------------------------------------------------------------------------------------------

            return this.incAmount;
        }

        public int getStartingY()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (start y axis of cycle)   
            //------------------------------------------------------------------------------------------------------------------

            return this.startingY;
        }

        public int getFinishCycleY()
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Accessor (finish y axis of cycle)   
            //------------------------------------------------------------------------------------------------------------------

            return this.finishY;
        }

        public void Draw(Graphics Destination)
        {
            //------------------------------------------------------------------------------------------------------------------
            // Purpose: Method (render stars)
            //------------------------------------------------------------------------------------------------------------------

            // Create brush and pen instances
            SolidBrush brush = new SolidBrush(Color.FromArgb(142, 134, 255));
            Pen pen = new Pen(Color.FromArgb(142, 134, 255));

            // Draw lines to scroll
            Destination.FillRectangle(brush, base.getX(), base.getY(), 600, 3);

            // Clean up
            brush.Dispose();
            pen.Dispose();
        }
    }
}
