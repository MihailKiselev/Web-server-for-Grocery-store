function sendPostRequest(url, data) {
  return axios.post(url, data)
    .then(response => {
      console.log('Response from server:', response.data);
      window.location.href = "Home Page HTML.html";
    })
    .catch(error => {
      console.error('Error during request:', error);
    });
}

document.querySelector('.sign_in_btn').addEventListener('click', function (event) {
  event.preventDefault();

  const email = document.getElementById('email_log').value;
  const password = document.getElementById('password_log').value;

  const data = {
    user_name: '',
    email: email,
    password: password
  };

  sendPostRequest('http://localhost:8080/login', data);
});

document.querySelector('.sign_up_btn').addEventListener('click', function (event) {
  event.preventDefault();

  const userName = document.getElementById('user_name_reg').value;
  const email = document.getElementById('email_reg').value;
  const password = document.getElementById('password_reg').value;

  const data = {
    user_name: userName,
    email: email,
    password: password
  };

  sendPostRequest('http://localhost:8080/register', data);
});
