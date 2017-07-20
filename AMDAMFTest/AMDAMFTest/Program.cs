using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using AMDAMF;

namespace AMDAMFTest
{
    class Program
    {
        static void Main(string[] args)
        {
            //Arithmetics test = new Arithmetics();
            
            //System.Diagnostics.Debug.WriteLine(test.Add(3, 2));

            AmdEncoder amdEncoder = new AmdEncoder();
            amdEncoder.TestEncode();
        }
    }
}
