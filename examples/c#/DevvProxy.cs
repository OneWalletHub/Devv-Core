using System;
using System.Text;
using System.IO;
using System.Net;
using System.Threading.Tasks;

namespace DevvProxy
{
    public class DevvProxy
    {

        public async Task<String> EncodeDevvScore(String username, String email, int score)
        {
            HttpWebRequest request = (HttpWebRequest)WebRequest.Create("http://192.168.1.118/record_score");

            string json = "{\"username\":\"" + username + "\"," +
                "\"email\":\"" + email + "\"," +
                "\"score\":\"" + score + "\"}";
            byte[] data = Encoding.ASCII.GetBytes(json);

            request.Method = "POST";
            request.ContentType = "application/json";
            Stream stream = await request.GetRequestStreamAsync();
            stream.Write(data, 0, data.Length);

            HttpWebResponse response = (HttpWebResponse)await request.GetResponseAsync();
            string responseString = new StreamReader(response.GetResponseStream()).ReadToEnd();

            if (response.StatusCode.Equals(HttpStatusCode.Created))
            {
                //success
                Stream r_stream = response.GetResponseStream();
                StreamReader reader = new StreamReader(stream);
                string text = reader.ReadToEnd();
                //need to parse out appliction-layer error message or public key
                return (text);
            }
            else if (response.StatusCode.Equals(HttpStatusCode.BadRequest))
            {
                //invalid data
                return ("Invalid request");
            }
            else if (response.StatusCode.Equals(HttpStatusCode.Conflict))
            {
                //this user/email already exists
                return ("Username already exists");
            }
            else
            {
                //something went wrong
                return ("Unknown Error");
            }
        }
    }
}
