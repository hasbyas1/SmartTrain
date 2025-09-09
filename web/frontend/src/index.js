import { createRoot } from "react-dom/client";
import './index.css';
import App from './App';

const container = document.getElementById("root");
const root = createRoot(container); // React 18 API
root.render(<App />);

// If you want your app to work offline and load faster, you can change
// unregister() to register() below. Note this comes with some pitfalls.
// Learn more about service workers: https://bit.ly/CRA-PWA
