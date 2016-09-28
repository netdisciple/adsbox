  /*
   *	ADSBox js functions 
   */
  var Colors = [
  "#E46544", "#E46643", "#E56744", "#E36844", "#E36844", "#E36942", "#E36942", "#E26942", 
  "#E46B44", "#E16A42", "#DF6C40", "#E06D41", "#DE6E40", "#DD6D3F", "#DE7040", "#DB723F", 
  "#DB733E", "#D9743C", "#D8763D", "#D8783C", "#D6783C", "#D57B3B", "#D57C3A", "#D17E39", 
  "#D17E39", "#D08039", "#CE8138", "#CC8436", "#CB8635", "#C98834", "#CA8935", "#C68D32", 
  "#C68D32", "#C38F31", "#C39131", "#C0952F", "#BE952E", "#BC982D", "#BE9A2F", "#BA9B2D", 
  "#B99E2C", "#B7A02C", "#B6A32B", "#B2A52B", "#B1A72B", "#AEAA2A", "#AEAB2A", "#ABAD2B", 
  "#AAAF2A", "#A8B22A", "#A7B32B", "#A3B629", "#A2B82A", "#9FBA2A", "#9FBD29", "#9CBE2A", 
  "#9BC02B", "#99C12B", "#98C42B", "#94C72A", "#93C92A", "#90CB29", "#90CC2A", "#8BCE2B", 
  "#8BCF2A", "#8BD42B", "#86D128", "#88D92C", "#83D528", "#83DB2A", "#83DC29", "#7FDE2A", 
  "#7FDE2A", "#7CE029", "#7CE12A", "#79E42A", "#77E42A", "#74E629", "#76E82B", "#72E829", 
  "#70E929", "#70EA2A", "#6FEB2B", "#6DED2A", "#6BED2A", "#6AEE2B", "#6AF02C", "#66F02C", 
  "#68F12F", "#65F22F", "#64F332", "#61F533", "#61F535", "#5DF536", "#5EF637", "#5BF83A", 
  "#5BF83C", "#5AF93F", "#57F941", "#57FB43", "#53FA45", "#53FD47", "#52FD49", "#4DFD4C", 
  "#4DFF4E", "#4CFF50", "#4BFF52", "#4AFF54", "#48FF59", "#47FD5B", "#47FE5F", "#43FE62", 
  "#42FE64", "#41FF65", "#3EFE69", "#3DFF6C", "#3AFE6E", "#38FE70", "#38FF73", "#34FF78", 
  "#34FF7A", "#33FF7E", "#32FF81", "#2FFF83", "#2EFE88", "#2DFE8A", "#2DFE8C", "#29FE90", 
  "#28FF92", "#27FF96", "#27FF9A", "#26FF9C", "#24FEA0", "#23FEA3", "#23FFA5", "#1EFFAA", 
  "#1FFFAD", "#1EFFAE", "#1DFEB2", "#1AFFB4", "#19FFB8", "#18FFB9", "#18FFBB", "#13FEC1", 
  "#14FFC2", "#14FEC6", "#12FFC8", "#0EFDCA", "#0EFFCF", "#0EFFD2", "#0EFFD2", "#0CFFD6", 
  "#0CFFD8", "#0AFFD9", "#0AFFDD", "#09FFDF", "#07FFE2", "#06FFE5", "#06FEE6", "#06FFE8", 
  "#07FFEB", "#05FFEC", "#04FFEF", "#04FFF0", "#02FFF1", "#03FFF4", "#01FFF5", "#02FFF8", 
  "#01FFF8", "#00FFF9", "#00FFFB", "#00FFFD", "#00FDFD", "#01FBFF", "#02FBFF", "#00F8FF", 
  "#00F8FF", "#00F6FF", "#01F6FF", "#00F3FF", "#00F1FF", "#01F0FF", "#00EFFE", "#01ECFF", 
  "#02EBFF", "#02E9FF", "#02E6FE", "#03E3FF", "#03E1FF", "#04E0FF", "#04DDFE", "#06DAFF", 
  "#05D9FE", "#06D7FF", "#06D4FE", "#09D0FF", "#08CEFF", "#09CBFF", "#07C9FE", "#0DC5FF", 
  "#0BC3FF", "#0EC0FF", "#0EBDFF", "#10BBFF", "#12B8FF", "#11B4FE", "#10B3FD", "#15AEFF", 
  "#15AEFF", "#17AAFF", "#17A8FE", "#19A4FF", "#1CA3FF", "#1B9EFF", "#199CFD", "#1F98FF", 
  "#1E97FF", "#1F93FE", "#1F90FE", "#228DFF", "#2389FF", "#2386FE", "#2285FD", "#2681FF", 
  "#2780FF", "#2A7EFF", "#2B7AFF", "#2D77FF", "#2C73FE", "#2B72FE", "#2A71FD", "#2F6CFF", 
  "#2E6BFF", "#3168FF", "#3165FE", "#3362FF", "#3661FF", "#365DFF", "#355CFE", "#375AFF", 
  "#3858FE", "#3956FF", "#3A53FF", "#3950FF", "#3A4FFF", "#3B4CFF", "#3C4BFE", "#3F49FF", 
  "#3C46FD", "#3E46FE", "#3E46FE", "#3F44FF", "#4042FF", "#4042FF", "#4040FE", "#4040FF"];

  var timeouts = 0;

  function update_clock(data) {
    var json_obj = JSON.parse(data);
    var date = new Date(json_obj.timestamp*1000); // in milliseconds
    var hours = date.getHours();
    var minutes = "0" + date.getMinutes();
    var seconds = "0" + date.getSeconds();
    document.getElementById("clock").innerHTML = 
      hours + ':' + minutes.substr(-2) + ':' + seconds.substr(-2);
  }

  function query_db(query, callback) {
      var xhr = new XMLHttpRequest();
      xhr.open("POST", "execute");
      xhr.send(query);
      var timeout = setTimeout(function() {
          xhr.abort();
          console.log("query_db() timeout expired (5s).");
          if (timeouts < 8) timeouts++;
        }, 5000);
      xhr.onreadystatechange = function() {
        if ( xhr.status == 200) {
          if (xhr.readyState == 4) {
  		        clearTimeout(timeout);
              if (timeouts > 0) timeouts--;
  		        callback(xhr.responseText); // callback is fired with the result as parameter
              update_clock(xhr.responseText); // update clock in navbar
            }
          } else {
  	         console.log('Error querying database: ' + xhr.statusText);
          }
      }
      xhr.onloadend = function(data) {}
      var list = document.getElementById("online").getElementsByTagName("img");
      for (i = 0; i < list.length; i++) {
        list[i].style.zIndex = -999;
      }
      if (timeouts==8) 
        document.getElementById("online_red").style.zIndex = 1;
      else if (timeouts>4) 
        document.getElementById("online_yellow").style.zIndex = 1;
      else 
        document.getElementById("online_green").style.zIndex = 1;
  }

  function readCookie(name) {
      var nameEQ = name + "=";
      var ca = document.cookie.split(';');
      for(var i=0;i < ca.length;i++) {
          var c = ca[i];
          while (c.charAt(0)==' ') c = c.substring(1,c.length);
          if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length,c.length);
      }
      return null;
  }

  function writeCookie(key, value) {
    var when = new Date();
    when.setFullYear(2100);
    document.cookie = key + "=" + value + "; expires=" + when.toString() + ";";	
  }
